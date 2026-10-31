// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "terragen/worlds.h"
#include "terrain.h"
#include "map.h"
#include "mapgen/treegen.h"
#include <algorithm>
#include <cmath>
#include <limits>

using namespace terragen;

namespace far {

// How far a tree's crown can reach from its trunk, in blocks.
// this far outside a level-0 tile are grown too, for what they lean into it.
static constexpr s32 REACH = 12;
// How tall a tree can grow above its base.
static constexpr s32 TALLEST = 48;
// A body's world code: the horizon's is the home's; another's is found
// from it (terragen/worlds.h), and then the atlas must hold the system's
// bodies. (A viewer of the home alone, the Planetarium, lays out the home.)
static WorldCode codeOf(const Horizon &horizon, const planet::Atlas &atlas, int body)
{
	if (body == planet::Atlas::HOME)
		return WorldCode::decode(horizon.code);
	const std::vector<terragen::BodyWorld> worlds =
			terragen::bodyWorlds(WorldCode::decode(horizon.code), atlas.body(planet::Atlas::HOME).params());
	if ((int)worlds.size() != atlas.bodies())
		throw BaseException("far: the atlas holds " + std::to_string(atlas.bodies()) + " bodies, the world code's system " +
				std::to_string(worlds.size()));
	return worlds.at(body).code;
}

Generator::Generator(const Horizon &horizon, const planet::Atlas &atlas, int body) :
	m_horizon(horizon),
	m_world(std::make_unique<World>(codeOf(horizon, atlas, body))),
	m_atlas(atlas),
	m_body(body),
	m_tiling(std::make_unique<Tiling>(atlas.body(body))),
	m_columns(std::make_unique<terra::Columns>(*m_world, horizon.content.blocks, horizon.water_level, m_atlas, body)),
	m_land(std::make_unique<terra::AtlasLand>(*m_columns, m_atlas))
{
	growCrowns();
}

void Generator::growCrowns()
{
	// each species' typical crown, from growing a few of its trees
	for (const terra::Species &s : m_horizon.content.species) {
		treegen::TreeDef def = terra::treeDef(s);
		s32 lows = 0, highs = 0, grown = 0;
		for (s32 k = 0; k < 8; k++) {
			MMVManip vm(nullptr);
			v3s16 base(k * 37, 1, k * 53);
			vm.addArea(VoxelArea(base - v3s16(REACH, 1, REACH), base + v3s16(REACH, TALLEST, REACH)));
			const VoxelArea &a = vm.m_area;
			for (s32 z = a.MinEdge.Z; z <= a.MaxEdge.Z; z++)
			for (s32 y = a.MinEdge.Y; y <= a.MaxEdge.Y; y++)
			for (s32 x = a.MinEdge.X; x <= a.MaxEdge.X; x++)
				vm.m_data[a.index(x, y, z)] = MapNode(y < base.Y ? m_horizon.content.blocks.stone : CONTENT_AIR);
			treegen::make_ltree(vm, base, def);
			s32 low = TALLEST, high = -1;
			for (s32 z = a.MinEdge.Z; z <= a.MaxEdge.Z; z++)
			for (s32 y = a.MinEdge.Y; y <= a.MaxEdge.Y; y++)
			for (s32 x = a.MinEdge.X; x <= a.MaxEdge.X; x++) {
				content_t c = vm.m_data[a.index(x, y, z)].getContent();
				if (c == s.leaves)
					low = std::min(low, y - base.Y);
				if (c == s.leaves || c == s.trunk)
					high = std::max(high, y - base.Y);
			}
			if (high < 0)
				continue;
			lows += std::min(low, high);
			highs += high;
			grown++;
		}
		m_crowns.push_back(grown ? Crown{ lows / grown, highs / grown } : Crown{ 0, -1 });
	}
}

Generator::~Generator() = default;

static Cell cellOf(const terra::Columns &columns, const terra::Column &c)
{
	const terra::Blocks &b = columns.blocks();
	Cell cell{};
	if (c.space) {
		cell.space = true;
		cell.ground = cell.water = columns.waterLevel(); // the wall's face toward it runs down to the sea
		return cell;
	}
	auto solid = [&](s32 y) {
		MapNode n = columns.node(c, y);
		content_t id = n.getContent();
		// a cave or spring opening at a cliff: show the rock it's cut in
		return id == CONTENT_AIR || id == b.water || id == b.lava ? MapNode(b.stone) : n;
	};
	cell.ground = c.height;
	cell.top = solid(c.height);
	cell.top_end = c.top_end;
	cell.filler_end = c.filler_end;
	cell.filler = c.top_end > c.filler_end ? solid(c.top_end) : cell.top;
	cell.rock = solid(std::min(c.filler_end, c.height - c.soil));
	cell.water = c.height;
	if (c.water > c.height) {
		cell.water = c.water;
		cell.liquid = columns.node(c, c.water);
	}
	return cell;
}

Cell Generator::column(s32 x, s32 z) const
{
	const terra::Column c = m_columns->at(x, z);
	if (c.space)
		throw BaseException("far::Generator::column: storage column (" + std::to_string(x) + ", " +
				std::to_string(z) + ") is no place on the planet");
	return cellOf(*m_columns, c);
}

/*
	An atlas tile (far/tiling.h): each cell the column of the atlas cell it
	stands for at the surface layer, as the generator builds it there
	(terra::AtlasLand's rule: the column at the cell's place; noise at its
	stored node), and its species' crown wherever the forest is dense enough
	(at every level: the game's trees are planted per map chunk, which a
	region's cells aren't). A cell the atlas has none of (trimmed, or past the
	grid) is space: nothing drawn, and its neighbours draw no cliff toward it
	(the region across the seam draws that ground).
*/
Tile Generator::build(const TileKey &key) const
{
	Tile tile;
	tile.key = key;
	tile.cells.resize((size_t)(TILE + 2) * (TILE + 2));
	const terragen::Atlas &atlas = m_atlas.body(m_body);
	for (s32 j = -1; j <= TILE; j++)
		for (s32 i = -1; i <= TILE; i++) {
			const terragen::Atlas::Cell cell = m_tiling->cell(key, i, j);
			Cell &out = tile.at(i, j);
			// past its grid's box a region's chart only carries on (and can come
			// back round into its territory): no cell, whatever exists() says
			if (!m_tiling->inGrid(key, cell) || !atlas.exists(cell)) {
				out = Cell{};
				out.space = true;
				out.ground = out.water = std::numeric_limits<s32>::max() / 2;
				continue;
			}
			const terra::Column c = m_columns->atPlace(m_atlas.place(planet::Atlas::Cell(m_body, cell)));
			out = cellOf(*m_columns, c);
			if (c.water > c.height)
				continue;
			const Cover cover = m_world->cover(c.at, c.ground);
			const content_t ground = m_columns->node(c, c.height).getContent();
			const terra::Species *s = terra::choose(m_horizon.content.species, terra::spotAt(c.ground, cover), ground);
			if (!s)
				continue;
			plant(*s, cover, c, out);
		}
	return tile;
}

// A species' crown on a cell, where its column's roll is under the forest's density.
void Generator::plant(const terra::Species &s, const Cover &cover, const terra::Column &c, Cell &out) const
{
	const Crown &crown = m_crowns[&s - m_horizon.content.species.data()];
	if (crown.high < 0 || (float)(cover.roll & 0xffff) / 65536.0f >= (float)cover.density / ONE)
		return;
	out.canopy = { c.height + 1 + crown.low, c.height + 1 + crown.high, MapNode(s.leaves) };
	if (crown.low > 0)
		out.trunk = { c.height + 1, c.height + crown.low, MapNode(s.trunk) };
}

} // namespace far
