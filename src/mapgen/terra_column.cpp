// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "terra_column.h"
#include "mapgen.h" // MAX_MAP_GENERATION_LIMIT
#include "planet/planet.h"
#include <algorithm>
#include <cmath>

using namespace terragen;

namespace terra {

Columns::Columns(const World &world, const Blocks &blocks, s16 water_level,
		const planet::Planet *planet) :
	m_world(world), m_planet(planet), m_blocks(blocks), m_water_level(water_level)
{
}

std::array<s16, 2> Columns::onWorld(s16 x, s16 z) const
{
	if (!m_planet)
		return { x, z };
	const std::array<int32_t, 2> w = m_planet->equirect(x, z, m_world.code().size);
	return { (s16)w[0], (s16)w[1] };
}

// The corner landmarks (a planet's 8 corners, where three faces meet): a bare
// stone peak over each, so the corner point is inside a mountain, never open
// ground.
static constexpr double LANDMARK_RADIUS = 32; // m
static constexpr double LANDMARK_HEIGHT = 72; // m above the sea, at the corner

Column Columns::at(s16 x, s16 z, fix sun) const
{
	if (!m_planet)
		return column(x, z, sun);
	const std::array<s16, 2> w = onWorld(x, z);
	Column c = column(w[0], w[1], sunlight(w[1]));
	const double d = m_planet->corner(x, z);
	if (d < LANDMARK_RADIUS) {
		const s32 peak = m_water_level + (s32)std::lround(LANDMARK_HEIGHT * (1 - d / LANDMARK_RADIUS));
		if (peak > c.height) {
			c.height = c.water = peak;
			c.water_above = c.lava = false;
			c.top = m_blocks.stone;
			c.soil = 0;
			c.top_end = c.filler_end = c.height;
		}
	}
	return c;
}

fix Columns::sunlight(s16 z) const
{
	fix v = m_world.v(z);
	return v < 0 || v >= ONE ? 0 : m_world.sunlight(v, std::nullopt);
}

Column Columns::column(s16 x, s16 z, fix sun) const
{
	Column c{};
	fix v = m_world.v(z);
	if (z >= m_world.spaceFrom()) {
		c.beyond = c.space = true;
		c.biome = Biome::Snow;
		c.height = c.water = -MAX_MAP_GENERATION_LIMIT - 1; // every block above it: air
		c.top = CONTENT_AIR;
		c.top_end = c.filler_end = c.ice_end = c.height;
		return c;
	}
	if (v < 0 || v >= ONE) {
		c.beyond = true;
		c.biome = Biome::Snow;
		c.height = c.water = MAX_MAP_GENERATION_LIMIT; // the polar wall: ice to the build limit
		c.top = m_blocks.ice;
		c.top_end = c.filler_end = c.height;
		c.ice_end = m_water_level;
		return c;
	}

	Ground g = m_world.ground(x, z, sun);
	const BiomeBlocks &b = m_blocks.biomes[(int)g.column.biome];
	c.biome = g.column.biome;
	c.ground = g;
	c.height = m_water_level + g.terrain;
	// dry ground has no water surface: put it at the ground
	c.water = g.kind == Water::None ? c.height : m_water_level + g.water;
	c.lava = g.kind == Water::Lava;
	c.water_above = g.kind != Water::None && !c.lava;
	c.strata = m_world.strata(x, z, g);

	// the soil: rock begins under it. A layer of plain stone isn't soil:
	// there the bedrock shows (peaks)
	bool riverbed = (g.kind == Water::River || g.kind == Water::Lake) && g.water > g.terrain &&
			b.riverbed != CONTENT_IGNORE;
	s32 top_depth = riverbed ? (b.riverbed == m_blocks.stone ? 0 : b.depth_riverbed)
			: (b.top == m_blocks.stone ? 0 : b.depth_top);
	c.soil = top_depth + (b.filler == m_blocks.stone ? 0 : b.depth_filler);
	c.soil_point = soilIndex(m_world.soil(x, z, g, *c.strata));

	// under a river or lake the top is the biome's riverbed
	bool bed = (g.kind == Water::River || g.kind == Water::Lake) && c.water > c.height;
	bool use_bed = bed && b.riverbed != CONTENT_IGNORE;
	c.top = use_bed ? b.riverbed : b.top;
	// rivers out of the mountains carry gold and tin in their gravel, here and there
	if (bed && g.kind == Water::River && c.strata->placer && ((x * 73856093) ^ (z * 19349663)) % 7 == 0)
		c.top = m_blocks.placer;
	c.top_end = c.height - (use_bed ? b.depth_riverbed : b.depth_top);
	c.filler_end = c.top_end - b.depth_filler;

	c.ice_end = b.water_top != CONTENT_IGNORE && c.height < m_water_level
			? m_water_level - b.depth_water_top : m_water_level;
	return c;
}

MapNode Columns::node(const Column &c, s16 x, s32 y, s16 z) const
{
	const Blocks &b = m_blocks;
	if (y > c.height) {
		if (y > c.water)
			return MapNode(CONTENT_AIR);
		if (c.lava)
			return MapNode(b.lava);
		// a frozen sea: the biome's water-top over open water
		if (y > c.ice_end && y <= m_water_level)
			return MapNode(b.biomes[(int)c.biome].water_top);
		return MapNode(b.water);
	}
	if (c.beyond)
		return MapNode(b.ice);

	s32 depth = c.height - y;
	MapNode n(b.stone); // inside the soil: painted below
	if (depth >= c.soil) {
		Rock rock = m_world.rock(*c.strata, depth - c.soil, x, y, z);
		Cave cave = m_world.cave(*c.strata, rock, depth, x, y, z, c.water_above);
		if (cave == Cave::Air)
			return MapNode(CONTENT_AIR);
		if (cave == Cave::Water)
			return MapNode(b.water);
		if (cave == Cave::Lava)
			return MapNode(b.lava);
		Ore ore = m_world.ore(*c.strata, rock, depth - c.soil, x, y, z);
		// a rock blends toward its neighbour (granite toward gneiss…): param2 is how far
		n = ore == Ore::None ? MapNode(b.rock[(int)rock], 0, m_world.blend(x, y, z)) : MapNode(b.ore[(int)ore]);
	}
	// the soil is painted over plain stone, top then filler
	if (n.getContent() == b.stone && y > c.filler_end) {
		content_t fill = y > c.top_end ? c.top : b.biomes[(int)c.biome].filler;
		if (fill != CONTENT_IGNORE)
			// soil carries what it's made of here as its param2
			return fill == b.soil ? MapNode(fill, 0, c.soil_point) : MapNode(fill);
	}
	return n;
}

} // namespace terra
