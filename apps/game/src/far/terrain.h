// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "far/globe.h"
#include "planet/atlas.h"
#include <optional>

#include "mapgen/terra_column.h"
#include "mapgen/terra_species.h"
#include "terragen/world.h"
#include <memory>
#include <string>
#include <vector>

/*
	The far view's world: what lies beyond the loaded map blocks, as blocks.
	Not a picture of the land but the land itself at a lower resolution: each
	cell is a column (or 2ᵏ×2ᵏ columns) made of the blocks the generator
	would place there, decided by the generator's own rule
	(terra::Columns::node) and grown with the world's own trees
	(terra::choose, treegen). So the far view and the real chunks can't
	disagree, and the seam between them is only a change of resolution.
	No client dependencies: built on worker threads, and in the unit tests.
*/
namespace far {

// What the server sends a client of a terra world (TOCLIENT_HORIZON): all
// the far view needs to build it.
struct Horizon
{
	std::string code;
	s16 water_level;
	terra::Blocks blocks;
	std::vector<terra::Species> species;
};


// The polar edge is ice to the build limit; the far view draws it this high
// above the sea.
constexpr s32 POLAR_WALL = 512;

// A vertical run of one block, low to high inclusive.
struct Run
{
	s32 low = 0, high = -1;
	MapNode node;
	bool empty() const { return high < low; }
};

// What a column (or cell of columns) shows from afar.
struct Cell
{
	bool space;      // past the polar wall, where the map is space: nothing to draw
	s32 ground;      // the top solid block
	MapNode top;     // that block
	// a cliff shows `top` down to top_end, `filler` down to filler_end, then `rock`
	s32 top_end, filler_end;
	MapNode filler, rock;
	s32 water;       // the liquid's surface, if above the ground
	MapNode liquid;
	Run trunk, canopy; // the trees standing here
};

// Cells per tile side. A tile at level k has cells 2ᵏ blocks wide.
constexpr s32 TILE = 32;

struct TileKey
{
	s32 level, x, z; // tile coordinates at its level
	s32 face = 0;    // on a globe (far/globe.h), the cube's face it's on
	s32 cell() const { return 1 << level; }
	s32 span() const { return TILE << level; }
	bool operator==(const TileKey &o) const { return level == o.level && x == o.x && z == o.z && face == o.face; }
};

struct Tile
{
	TileKey key;
	// (TILE + 2)² cells: one of border on every side, for the faces between tiles
	std::vector<Cell> cells;
	const Cell &at(s32 i, s32 j) const { return cells[(size_t)(j + 1) * (TILE + 2) + (i + 1)]; }
	Cell &at(s32 i, s32 j) { return cells[(size_t)(j + 1) * (TILE + 2) + (i + 1)]; }
};

/*
	Builds tiles. Const and thread-safe: workers share one.
	Level 0 is the world column for column, trees and all: each tree the
	world grows is grown again (treegen, seeded by its position as the
	world's are), clipped to the chunk it was planted in. Coarser levels
	sample each cell's centre column and stand its species' crown wherever
	the forest is dense enough.
*/
class Generator
{
public:
	// A flat world's (the Lab's): columns are the world model's, block for block
	explicit Generator(const Horizon &horizon);
	// An atlas planet's: its tiles are a globe's (far/globe.h), each cell
	// the column the atlas's generator builds at that latitude and longitude
	Generator(const Horizon &horizon, const planet::Atlas &atlas);
	~Generator();

	const Horizon &horizon() const { return m_horizon; }
	const terragen::World &world() const { return *m_world; }

	Tile build(const TileKey &key) const;

	// The column at (x, z) without its trees: what level 0 starts from.
	Cell column(s32 x, s32 z) const;

private:
	struct Crown
	{
		s32 low, high; // above the ground, typical
	};

	void growCrowns();
	Tile buildOnGlobe(const TileKey &key) const;
	void growExact(Tile &tile) const;
	void growCoarse(Tile &tile) const;
	const terra::Species *speciesAt(const terra::Column &c, s32 x, s32 z) const;

	Horizon m_horizon;
	terragen::Cover coverAt(s32 x, s32 z, const terragen::Ground &ground) const;

	std::unique_ptr<terragen::World> m_world;
	std::optional<planet::Atlas> m_atlas;
	std::optional<Globe> m_globe; // an atlas's tiles
	std::unique_ptr<terra::Columns> m_columns;
	std::vector<Crown> m_crowns; // per species
};

} // namespace far
