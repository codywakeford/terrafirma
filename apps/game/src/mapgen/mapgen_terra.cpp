// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "mapgen_terra.h"
#include "mapgen/terra_column.h"
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

// The game's blocks for everything the generator places. Throws if the game
// lacks one: every rock and ore terragen can place must be a block.
terra::Blocks terraBlocks(const BiomeManager *bmgr, const NodeDefManager *ndef)
{
	content_t stone = ndef->getId("mapgen_stone"), water = ndef->getId("mapgen_water_source");
	if (stone == CONTENT_IGNORE || water == CONTENT_IGNORE)
		throw BaseException("terra map generator: the game has no 'mapgen_stone' or 'mapgen_water_source' alias");
	terra::Blocks blocks;
	for (int i = 0; i < (int)terragen::Biome::Count; i++) {
		auto *b = (const ::Biome *)bmgr->getByName(BIOME_NAMES[i]);
		if (!b)
			throw BaseException(std::string("terra map generator: the game registers no biome called '") +
					BIOME_NAMES[i] + "'");
		blocks.biomes[i] = { b->c_top, b->c_filler, b->c_riverbed, b->c_water_top,
			b->depth_top, b->depth_filler, b->depth_riverbed, b->depth_water_top };
	}
	auto need = [&](const std::string &alias) {
		content_t c = ndef->getId(alias);
		if (c == CONTENT_IGNORE)
			throw BaseException("terra map generator: the game has no '" + alias + "' alias" +
					(alias == "mapgen_ice" ? " for the polar wall" : ""));
		return c;
	};
	blocks.stone = stone;
	blocks.water = water;
	blocks.ice = need("mapgen_ice");
	blocks.lava = need("mapgen_lava_source");
	blocks.placer = need("mapgen_placer");
	blocks.soil = need("mapgen_soil");
	for (int i = 0; i < (int)terragen::Rock::Count; i++)
		blocks.rock[i] = need(std::string("mapgen_rock_") + ROCK_NAMES[i]);
	blocks.ore[0] = CONTENT_IGNORE;
	for (int i = 1; i < (int)terragen::Ore::Count; i++)
		blocks.ore[i] = need(std::string("mapgen_ore_") + ORE_NAMES[i]);
	return blocks;
}

MapgenTerra::MapgenTerra(MapgenTerraParams *params, EmergeParams *emerge) :
	MapgenBasic(MAPGEN_TERRA, params, emerge),
	m_world(decodeOrFail(params->code)),
	m_planet(params->planet_half ? std::optional<planet::Planet>(params->planet_half) : std::nullopt),
	m_columns(m_world, terraBlocks(m_bmgr, ndef), water_level, m_planet ? &*m_planet : nullptr)
{
	for (int i = 0; i < (int)terragen::Biome::Count; i++)
		m_biomes[i] = (::Biome *)m_bmgr->getByName(BIOME_NAMES[i]);
	c_lava_source = m_columns.blocks().lava;
	m_column.resize((size_t)csize.X * csize.Z);
}

bool MapgenTerra::groundAt(s16 x, s16 z, terragen::Ground &ground) const
{
	const std::array<s16, 2> w = m_columns.onWorld(x, z); // on a planet, its latitude and longitude
	x = w[0];
	z = w[1];
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
	const v3s32 &em = vm->m_area.getExtent();
	s16 stone_max = -MAX_MAP_GENERATION_LIMIT;
	size_t index = 0;

	for (s16 z = node_min.Z; z <= node_max.Z; z++) {
		// sunlight depends only on latitude: once per row
		fix sun = m_columns.sunlight(z);
		for (s16 x = node_min.X; x <= node_max.X; x++, index++) {
			const terra::Column &c = m_column[index] = m_columns.at(x, z, sun);
			u32 vi = vm->m_area.index(x, node_min.Y - 1, z);
			for (s16 y = node_min.Y - 1; y <= node_max.Y + 1; y++) {
				if (vm->m_data[vi].getContent() == CONTENT_IGNORE) {
					// every block from the one rule the far view also draws with
					vm->m_data[vi] = m_columns.node(c, x, y, z);
					if (y <= c.height && y > stone_max)
						stone_max = y;
				}
				VoxelArea::add_y(em, vi, 1);
			}
		}
	}
	return stone_max;
}

void MapgenTerra::paintSurface()
{
	size_t index = 0;
	for (s16 z = node_min.Z; z <= node_max.Z; z++)
	for (s16 x = node_min.X; x <= node_max.X; x++, index++)
		biomemap[index] = m_biomes[(int)m_column[index].biome]->index;
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
