// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "code.h"
#include "fixed.h"
#include "geology.h"
#include <cstdint>
#include <memory>
#include <vector>

namespace terragen {

// The coarse grid's resolution, whatever the world size (8k → 32-block cells).
constexpr int32_t GRID = 256;

/*
	The whole world at coarse resolution, with its water worked out: where
	every cell drains, how much rain passes through it, where it pools into
	lakes, and how far running water has worn the land down. Built once per
	world code (see gridFor) and shared. Cells are row-major, row 0 = north;
	columns wrap east–west; polar rows are walls water never crosses.
*/
struct Grid
{
	fix sea_offset;                // subtracted from raw elevation: the Land share is exact
	std::vector<fix> base;         // elevation after the sea offset, before erosion
	std::vector<fix> eroded;       // after erosion
	std::vector<fix> filled;       // the water surface: = eroded on dry land, above it on lakes
	std::vector<int32_t> receiver; // the cell this one drains into; −1 = the sea (or a polar wall)
	std::vector<int64_t> drained;  // rain-weighted area draining through, in fixed cell units
	std::vector<uint8_t> lake;     // 1 where water pools at least a block deep
	// On lake cells, the lake's one surface: its outlet, the lowest filled
	// level in the basin. (The fill's jitter leaves the rest a hair higher,
	// which in blocks would be steps that spill across the lake.)
	std::vector<fix> lake_level;
	std::vector<uint8_t> polar;    // 1 in the polar edge rows
	int64_t river_threshold;       // drained area that makes a river (fixed cell units)

	// geology (geology.h): the plates under it, each cell's province, the
	// volcanoes, and which volcanoes' slopes reach each cell
	// (volcano_list[volcano_start[i] .. volcano_start[i + 1]])
	Tectonics tectonics;
	std::vector<Province> province;
	std::vector<Volcano> volcanoes;
	std::vector<uint32_t> volcano_start;
	std::vector<uint16_t> volcano_list;

	bool river(int32_t i) const { return !polar[i] && base[i] >= 0 && drained[i] >= river_threshold; }

	static int32_t index(int32_t cx, int32_t cz) { return cz * GRID + cx; }
};

// The grid for a world code, built on first use and shared after (thread-safe).
std::shared_ptr<const Grid> gridFor(const WorldCode &code);

// The landmass field: where land is, before the sea is placed.
fix landmass(const WorldCode &code, fix u, fix v);
// Elevation with the sea at `sea_offset` of the landmass field: 0 is the
// coastline, mountains rise from it inland.
// `belt` (0..1) is how strongly mountains rise here, `relief` what plate
// boundaries add (rifts, ridges, trenches); both from Tectonics.
fix elevation(const WorldCode &code, fix u, fix v, fix sea_offset, fix belt, fix relief);
// Moisture, ~−1 arid .. 1 soaked: decides where rain falls.
fix moisture(const WorldCode &code, fix u, fix v);

// Distinct noise streams, so the fields are independent.
enum Stream : uint32_t { CONTINENT = 0, COAST = 3, RIDGE = 7, HEAT = 13, WET = 29, DETAIL = 41 };

} // namespace terragen
