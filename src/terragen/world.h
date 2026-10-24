// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "code.h"
#include "fixed.h"
#include "geology.h"
#include "grid.h"
#include "map.h"
#include <memory>
#include <optional>
#include <string>

namespace terragen {

// The biome classes terragen assigns. The game registers a Lua biome with
// each name (games/terrafirma/mods/tf_world); the order is what the preview
// sends, with the names alongside.
enum class Biome : uint8_t {
	Deep, Ocean, SeaIce, Beach, Desert, Savanna, Grassland, Forest, Jungle,
	Taiga, Tundra, Rock, Snow, Monsoon, Mediterranean,
	Count
};

extern const char *const BIOME_NAMES[(int)Biome::Count];

struct Column
{
	fix elevation; // −1..~1.2, 0 = sea level; erosion included
	fix heat;      // ~−1 frozen .. 1 scorching
	fix wet;       // ~−1 arid .. 1 soaked: log₂ of `rain`, halved
	fix rain;      // the year's rain, relative to the median over land (1)
	fix season;    // when it falls: 1 all in summer .. −1 all in winter (0 even)
	Biome biome;
	bool polar;    // inside the impassable polar edge
};

enum class Water : uint8_t { None, Sea, Lake, River, Lava };

// What can grow on a column (World::cover): how densely, and the ground's
// lie. Which plants grow is the game's choice.
struct Cover
{
	fix density;   // 0..1: bare to closed forest
	fix slope;     // rise over run
	fix fertility; // 0..1: what the ground supplies, less what rain washes out
	bool bank;     // beside a river or lake
	bool tree;     // a tree stands here (one spot per small cell, if the roll allows)
	uint32_t roll; // a per-column random number, for the game's choices
};

// Soil as a composition (soil.cpp): its mineral part on the texture
// triangle, and organic matter as a share of the whole.
struct Soil
{
	fix sand, silt, clay; // shares of the mineral part, summing to ~1
	fix organic;          // 0..1 of the soil
};
// The compositions a soil block can hold (its param2): sand and clay in
// tenths, silt the rest, at three levels of organic matter.
constexpr int32_t SOIL_ORGANIC = 3;
constexpr int32_t SOIL_POINTS = 66 * SOIL_ORGANIC;
uint8_t soilIndex(const Soil &s);
Soil soilPoint(uint8_t index); // throws std::invalid_argument past SOIL_POINTS
// How many steps a rock blends toward its neighbour (its param2).
constexpr int32_t ROCK_BLENDS = 16;

// What a cave holds where it's carved.
enum class Cave : uint8_t { None, Air, Water, Lava };

// A column as it is built: ground and water heights in blocks, relative to
// sea level (the map generator adds its water level).
struct Ground
{
	int32_t terrain; // the top solid block
	int32_t water;   // the water surface, if `kind` isn't None
	Water kind;
	Column column;
};

/*
	The world model: a pure function of the world code. Columns are addressed
	by (u, v) ∈ [0,1)² — u wraps east–west, v runs north (0) to south (1) —
	or by block coordinates (x, z), centred on the origin.
*/
class World
{
public:
	explicit World(const WorldCode &code);

	const WorldCode &code() const { return m_code; }
	const Grid &grid() const { return *m_grid; }

	// Latitude of row v, radians, positive north (±π/2 at the edges).
	static fix latitude(fix v);
	// Sunlight reaching latitude row v: on `date` (0..1), or the yearly mean.
	fix sunlight(fix v, std::optional<fix> date) const;
	Column column(fix u, fix v, fix sunlight) const;

	// The finished column at block (x, z), water included. `sunlight` for
	// its row (see sunlight()). Not for columns beyond the polar edge.
	Ground ground(int32_t x, int32_t z, fix sunlight) const;

	// The air temperature in °C at block (x, z) on `date`, at `hour` of the
	// day (0..1): the season's climate there plus a daily swing that peaks
	// mid-afternoon and is wider where it's dry. Not beyond the polar edge.
	fix temperature(int32_t x, int32_t z, fix date, fix hour) const;

	// Block coordinates → (u, v). v may fall outside [0,1): beyond the poles.
	fix u(int32_t x) const;
	fix v(int32_t z) const;

	// The map's geometry on the planet (map.h).
	const Map &map() const { return m_map; }

	// Where space begins: north of this z, past the polar wall, the map is
	// empty (a place for what's in orbit: tf_flight's bubbles), sealed from
	// the planet by the wall, which rises to the build limit.
	int32_t spaceFrom() const { return m_code.size / 2 + SPACE_WALL; }
	static constexpr int32_t SPACE_WALL = 256; // blocks of polar ice between the planet and space

	// Surface height in blocks for an elevation, with fine detail at (u, v).
	int32_t surface(fix elevation, fix u, fix v) const;

	// Geology (geology.cpp). A column's strata, then by block: the rock at a
	// depth below the bedrock's top, the ore there if any, and whether a
	// cave is carved there (`water_above`: under a sea, lake or river).
	Strata strata(int32_t x, int32_t z, const Ground &ground) const;
	Rock rock(const Strata &s, int32_t depth, int32_t x, int32_t y, int32_t z) const;
	Ore ore(const Strata &s, Rock rock, int32_t depth, int32_t x, int32_t y, int32_t z) const;
	Cave cave(const Strata &s, Rock rock, int32_t depth, int32_t x, int32_t y, int32_t z, bool water_above) const;
	// A grid field (Grid's per-cell vectors) at (u, v), between cell centres.
	fix between(const std::vector<fix> &field, fix u, fix v) const;
	// The coarse cell a column falls in.
	int32_t cell(fix u, fix v) const;

	// What the soil is made of here (soil.cpp), and how far a rock at a block
	// blends toward its neighbour (0 .. ROCK_BLENDS − 1).
	Soil soil(int32_t x, int32_t z, const Ground &ground, const Strata &strata) const;
	uint8_t blend(int32_t x, int32_t y, int32_t z) const;

	// What can grow here (vegetation.cpp).
	Cover cover(int32_t x, int32_t z, const Ground &ground) const;

private:
	fix erosion(fix u, fix v) const;
	// the plates' mountain belt and relief at (u, v), between cell centres
	void tectonic(fix u, fix v, fix &belt, fix &relief) const;
	// how much every nearby volcano raises (x, z), and a crater's lava surface
	// (absolute blocks) if any
	// `floor` is the crater's floor (absolute) inside a crater, built level from the vent
	void volcanic(int32_t x, int32_t z, int32_t &raise, int32_t &lava, int32_t &floor) const;

	WorldCode m_code;
	Map m_map;
	std::shared_ptr<const Grid> m_grid;
};

// Rows within this share of the north and south edges are the polar edge.
constexpr fix POLAR_EDGE = 2294; // 0.035

// Blocks per unit of elevation, above and below the sea.
constexpr int32_t LAND_SCALE = 160;
constexpr int32_t SEA_SCALE = 64;

} // namespace terragen
