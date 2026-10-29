// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <array>
namespace planet { class Atlas; }

#include "irrlichttypes.h"
#include "mapnode.h"
#include "terragen/world.h"
#include <optional>

/*
	What the terra generator puts in a column, decided once: its ground,
	soil, water and ice, as blocks. The generator fills chunks from it, and
	the far view draws the columns it hasn't loaded from it, so the two can't
	disagree. Pure: content ids in, no map or node definitions.
*/
namespace terra {

// The blocks the game gives one terragen biome (its Lua biome's nodes).
struct BiomeBlocks
{
	content_t top, filler, riverbed, water_top;
	s16 depth_top, depth_filler, depth_riverbed, depth_water_top;
};

// Every block the generator places, by role.
struct Blocks
{
	BiomeBlocks biomes[(int)terragen::Biome::Count];
	content_t stone, water, ice, lava, placer;
	content_t soil; // soil as a composition: its param2 is the column's soil point
	content_t rock[(int)terragen::Rock::Count];
	content_t ore[(int)terragen::Ore::Count]; // [None] is CONTENT_IGNORE
};

struct Column
{
	bool beyond;    // past the polar edge: ice to the build limit
	bool space;     // past the polar wall (World::spaceFrom): nothing at all
	terragen::Biome biome;
	terragen::Ground ground; // terragen's column (not beyond): for what grows here
	s32 height;     // the top solid block, absolute y
	s32 water;      // the water (or lava) surface; == height when dry
	bool lava;      // the surface liquid is lava
	bool water_above; // under a sea, lake or river (not lava)
	std::optional<terragen::Strata> strata; // not beyond
	s32 soil;       // soil depth: rock begins this far under the surface
	u8 soil_point;  // what the soil is made of here (terragen::soilPoint), for Blocks::soil's param2

	// the soil the surface is painted with: `top` down to (not including)
	// top_end, the filler down to filler_end
	content_t top;
	s32 top_end, filler_end;
	// a frozen sea: water above ice_end (and above the ground) is the
	// biome's water_top; ice_end is water_level when nothing freezes
	s32 ice_end;
};

// The world model's block at a latitude and longitude (radians): its map is
// equirectangular, x longitude from 0, z latitude, poles at ±size/2.
std::array<int32_t, 2> equirect(double latitude, double longitude, int32_t size);

class Columns
{
public:
	// `atlas`: the world's planet, if it's one (src/planet/atlas.h), and
	// which of its bodies `world` is. Then a column of the map is the world
	// model's column at its latitude and longitude on the body: the whole
	// world, poles and all, on the sphere. A column of another body's tile
	// throws.
	Columns(const terragen::World &world, const Blocks &blocks, s16 water_level,
			const planet::Atlas *atlas, int body);
	int body() const { return m_body; }

	const Blocks &blocks() const { return m_blocks; }
	s16 waterLevel() const { return m_water_level; }

	// `sun`: World::sunlight for the column's row (it depends only on z); 0
	// beyond the polar edge. On a planet it's found from where the column is.
	Column at(s16 x, s16 z, terragen::fix sun) const;
	// The column at a latitude and longitude (radians): an atlas's nodes are
	// built from their place (planet::Atlas::place), node by node.
	Column atPlace(double latitude, double longitude) const;
	terragen::fix sunlight(int32_t z) const;

	// The world model's block for a column of the map: itself, or on a planet
	// the one at its latitude and longitude (the world model's map is
	// equirectangular: x longitude, z latitude, poles at ±size/2).
	std::array<int32_t, 2> onWorld(s16 x, s16 z) const;
	// The world model's block at a latitude and longitude (radians).
	std::array<int32_t, 2> onWorldAt(double latitude, double longitude) const;

	// The block the generator leaves at (x, y, z) of column `c`: rock, ore
	// and caves below the soil, the soil painted over it, water (frozen at
	// the top of a cold sea) and air above. Before decorations, dust and the
	// game's own planting.
	// x, z: the column's place for what varies node to node (on a planet, its
	// spot: planet::Atlas::spot).
	MapNode node(const Column &c, s32 x, s32 y, s32 z) const;

private:
	Column column(int32_t x, int32_t z, terragen::fix sun) const;
	Column nothing() const; // a column of no world: air, for storage that isn't a place

	const terragen::World &m_world;
	const planet::Atlas *m_atlas;
	int m_body;
	// a storage column's place on the body (none: seam); throws for another body's
	std::optional<std::array<double, 2>> placeOf(s16 x, s16 z) const;
	Blocks m_blocks;
	s16 m_water_level;
};

} // namespace terra
