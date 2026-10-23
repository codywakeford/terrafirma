// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "mapgen_terra.h"
#include <optional>
#include "emerge.h"
#include "exceptions.h"
#include "mg_biome.h"
#include "mg_decoration.h"
#include "mg_ore.h"
#include "nodedef.h"
#include "settings.h"
#include "voxel.h"

using namespace terragen;

void MapgenTerraParams::readParams(const Settings *settings)
{
	settings->getNoEx("mgterra_code", code);
}

void MapgenTerraParams::writeParams(Settings *settings) const
{
	settings->set("mgterra_code", code);
}

static WorldCode decodeOrFail(const std::string &code)
{
	if (code.empty())
		throw BaseException("terra map generator: this world has no world code (mgterra_code in map_meta.txt)");
	try {
		return WorldCode::decode(code);
	} catch (const CodeError &e) {
		throw BaseException(std::string("terra map generator: ") + e.what());
	}
}

MapgenTerra::MapgenTerra(MapgenTerraParams *params, EmergeParams *emerge) :
	MapgenBasic(MAPGEN_TERRA, params, emerge),
	m_world(decodeOrFail(params->code))
{
	for (int i = 0; i < (int)terragen::Biome::Count; i++) {
		m_biomes[i] = (::Biome *)m_bmgr->getByName(BIOME_NAMES[i]);
		if (!m_biomes[i])
			throw BaseException(std::string("terra map generator: the game registers no biome called '") +
					BIOME_NAMES[i] + "'");
	}
	c_ice = ndef->getId("mapgen_ice");
	if (c_ice == CONTENT_IGNORE)
		throw BaseException("terra map generator: the game has no 'mapgen_ice' alias for the polar wall");
	// every rock and ore terragen can place must be a block in the game
	auto need = [&](const std::string &alias) {
		content_t c = ndef->getId(alias);
		if (c == CONTENT_IGNORE)
			throw BaseException("terra map generator: the game has no '" + alias + "' alias");
		return c;
	};
	for (int i = 0; i < (int)terragen::Rock::Count; i++)
		c_rock[i] = need(std::string("mapgen_rock_") + ROCK_NAMES[i]);
	c_ore[0] = CONTENT_IGNORE;
	for (int i = 1; i < (int)terragen::Ore::Count; i++)
		c_ore[i] = need(std::string("mapgen_ore_") + ORE_NAMES[i]);
	c_placer = need("mapgen_placer");
	c_lava_source = need("mapgen_lava_source");

	size_t columns = (size_t)csize.X * csize.Z;
	m_surface.resize(columns);
	m_water.resize(columns);
	m_kind.resize(columns);
	m_column_biome.resize(columns);
	m_polar.resize(columns);
	m_placer.resize(columns);
}

bool MapgenTerra::groundAt(s16 x, s16 z, terragen::Ground &ground) const
{
	fix v = m_world.v(z);
	if (v < 0 || v >= ONE)
		return false; // beyond the polar edge
	ground = m_world.ground(x, z, m_world.sunlight(v, std::nullopt));
	return !ground.column.polar;
}

void MapgenTerra::makeChunk(BlockMakeData *data)
{
	assert(data->vmanip);
	assert(data->nodedef);

	this->generating = true;
	this->vm = data->vmanip;
	this->ndef = data->nodedef;

	v3s16 blockpos_min = data->blockpos_min;
	v3s16 blockpos_max = data->blockpos_max;
	node_min = blockpos_min * MAP_BLOCKSIZE;
	node_max = (blockpos_max + v3s16(1, 1, 1)) * MAP_BLOCKSIZE - v3s16(1, 1, 1);
	full_node_min = (blockpos_min - 1) * MAP_BLOCKSIZE;
	full_node_max = (blockpos_max + 2) * MAP_BLOCKSIZE - v3s16(1, 1, 1);

	blockseed = getBlockSeed2(full_node_min, seed);

	generateTerrain();
	paintSurface();
	updateHeightmap(node_min, node_max);

	// Caves and dungeons arrive with terragen's geology (a later sprint).

	if (flags & MG_ORES)
		m_emerge->oremgr->placeAllOres(this, blockseed, node_min, node_max);
	if (flags & MG_DECORATIONS)
		m_emerge->decomgr->placeAllDecos(this, blockseed, node_min, node_max);
	dustTopNodes();

	updateLiquid(&data->transforming_liquid, full_node_min, full_node_max);
	if (flags & MG_LIGHT)
		calcLighting(node_min - v3s16(0, 1, 0), node_max + v3s16(0, 1, 0),
			full_node_min, full_node_max);

	this->generating = false;
}

s16 MapgenTerra::generateTerrain()
{
	MapNode n_air(CONTENT_AIR), n_stone(c_stone), n_water(c_water_source), n_ice(c_ice), n_lava(c_lava_source);
	const v3s32 &em = vm->m_area.getExtent();
	s16 stone_max = -MAX_MAP_GENERATION_LIMIT;
	size_t index = 0;

	for (s16 z = node_min.Z; z <= node_max.Z; z++) {
		fix v = m_world.v(z);
		bool beyond = v < 0 || v >= ONE;
		// sunlight depends only on latitude: once per row
		fix sun = beyond ? 0 : m_world.sunlight(v, std::nullopt);
		for (s16 x = node_min.X; x <= node_max.X; x++, index++) {
			s32 height, water;
			bool lava = false, water_above = false;
			std::optional<Strata> strata;
			s32 soil = 0;
			if (beyond) {
				height = MAX_MAP_GENERATION_LIMIT; // the polar wall: ice to the build limit
				water = height;
				m_kind[index] = Water::None;
				m_column_biome[index] = terragen::Biome::Snow;
				m_polar[index] = true;
			} else {
				Ground g = m_world.ground(x, z, sun);
				height = water_level + g.terrain;
				// dry ground has no water surface: put it below the ground
				water = g.kind == Water::None ? height : water_level + g.water;
				m_kind[index] = g.kind;
				m_column_biome[index] = g.column.biome;
				m_polar[index] = g.column.polar;
				lava = g.kind == Water::Lava;
				water_above = g.kind != Water::None && !lava;
				strata = m_world.strata(x, z, g);
				m_placer[index] = strata->placer;
				// the soil, which paintSurface puts in: rock begins under it. A
				// layer of plain stone isn't soil: there the bedrock shows (peaks)
				const ::Biome *b = m_biomes[(int)g.column.biome];
				bool riverbed = (g.kind == Water::River || g.kind == Water::Lake) && g.water > g.terrain &&
						b->c_riverbed != CONTENT_IGNORE;
				s32 top = riverbed ? (b->c_riverbed == c_stone ? 0 : b->depth_riverbed) : (b->c_top == c_stone ? 0 : b->depth_top);
				soil = top + (b->c_filler == c_stone ? 0 : b->depth_filler);
			}
			m_surface[index] = height;
			m_water[index] = water;

			u32 vi = vm->m_area.index(x, node_min.Y - 1, z);
			for (s16 y = node_min.Y - 1; y <= node_max.Y + 1; y++) {
				if (vm->m_data[vi].getContent() == CONTENT_IGNORE) {
					if (y <= height) {
						s32 depth = height - y;
						if (beyond || depth < soil) {
							vm->m_data[vi] = beyond ? n_ice : n_stone; // the soil is painted over this
						} else {
							Rock rock = m_world.rock(*strata, depth - soil, x, y, z);
							Cave cave = m_world.cave(*strata, rock, depth, x, y, z, water_above);
							if (cave == Cave::Air)
								vm->m_data[vi] = n_air;
							else if (cave == Cave::Water)
								vm->m_data[vi] = n_water;
							else if (cave == Cave::Lava)
								vm->m_data[vi] = n_lava;
							else {
								terragen::Ore ore = m_world.ore(*strata, rock, depth - soil, x, y, z);
								vm->m_data[vi] = MapNode(ore == terragen::Ore::None ? c_rock[(int)rock] : c_ore[(int)ore]);
							}
						}
						if (y > stone_max)
							stone_max = y;
					} else if (y <= water) {
						vm->m_data[vi] = lava ? n_lava : n_water;
					} else {
						vm->m_data[vi] = n_air;
					}
				}
				VoxelArea::add_y(em, vi, 1);
			}
		}
	}
	return stone_max;
}

void MapgenTerra::paintSurface()
{
	const v3s32 &em = vm->m_area.getExtent();
	size_t index = 0;
	for (s16 z = node_min.Z; z <= node_max.Z; z++)
	for (s16 x = node_min.X; x <= node_max.X; x++, index++) {
		::Biome *b = m_biomes[(int)m_column_biome[index]];
		biomemap[index] = b->index;
		s32 surface = m_surface[index];
		if (surface >= MAX_MAP_GENERATION_LIMIT)
			continue; // polar wall: all ice

		// top and filler, from the surface down, inside this chunk; under a
		// river or lake the top is the biome's riverbed
		bool bed = (m_kind[index] == Water::River || m_kind[index] == Water::Lake) && m_water[index] > surface;
		content_t top = bed && b->c_riverbed != CONTENT_IGNORE ? b->c_riverbed : b->c_top;
		// rivers out of the mountains carry gold and tin in their gravel, here and there
		if (bed && m_kind[index] == Water::River && m_placer[index] && ((x * 73856093) ^ (z * 19349663)) % 7 == 0)
			top = c_placer;
		s32 top_end = surface - (bed && b->c_riverbed != CONTENT_IGNORE ? b->depth_riverbed : b->depth_top);
		s32 filler_end = top_end - b->depth_filler;
		s32 y_start = std::min<s32>(surface, node_max.Y);
		u32 vi = vm->m_area.index(x, y_start, z);
		for (s32 y = y_start; y > filler_end && y >= node_min.Y; y--) {
			content_t c = y > top_end ? top : b->c_filler;
			if (c != CONTENT_IGNORE && vm->m_data[vi].getContent() == c_stone)
				vm->m_data[vi] = MapNode(c);
			VoxelArea::add_y(em, vi, -1);
		}

		// a frozen sea: the biome's water-top node over open water
		if (b->c_water_top != CONTENT_IGNORE && surface < water_level) {
			s32 ice_end = water_level - b->depth_water_top;
			for (s32 y = std::min<s32>(water_level, node_max.Y); y > ice_end && y > surface && y >= node_min.Y; y--) {
				u32 wi = vm->m_area.index(x, y, z);
				if (vm->m_data[wi].getContent() == c_water_source)
					vm->m_data[wi] = MapNode(b->c_water_top);
			}
		}
	}
}

int MapgenTerra::getGroundLevelAtPoint(v2s16 p)
{
	Ground g;
	if (!groundAt(p.X, p.Y, g))
		return MAX_MAP_GENERATION_LIMIT;
	return water_level + g.terrain;
}

int MapgenTerra::getSpawnLevelAtPoint(v2s16 p)
{
	Ground g;
	if (!groundAt(p.X, p.Y, g) || g.kind != Water::None)
		return MAX_MAP_GENERATION_LIMIT; // in the sea, a lake or a river: the engine looks elsewhere
	return water_level + g.terrain + 2; // above the top node and any dust
}
