// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "world.h"
#include "noise.h"
#include "sun.h"
#include <algorithm>

namespace terragen {

const char *const BIOME_NAMES[(int)Biome::Count] = {
	"deep", "ocean", "seaice", "beach", "desert", "savanna", "grassland",
	"forest", "jungle", "taiga", "tundra", "rock", "snow",
};

World::World(const WorldCode &code) : m_code(code), m_grid(gridFor(code))
{
}

fix World::latitude(fix v)
{
	return mul(HALF - v, PI);
}

fix World::sunlight(fix v, std::optional<fix> date) const
{
	fix latitude = World::latitude(v);
	fix yearly = annualInsolation(latitude, m_code.tilt);
	if (!date)
		return yearly;
	// that day's sun, plus a share of the year's (land and sea store heat)
	fix today = insolation(latitude, declination(m_code.tilt, *date));
	return mul(today, ratio(6, 10)) + mul(yearly, ratio(4, 10));
}

static Biome classify(fix e, fix heat, fix wet)
{
	if (e < 0)
		return heat < -ratio(75, 100) ? Biome::SeaIce : e < -ratio(18, 100) ? Biome::Deep : Biome::Ocean;
	if (e < ratio(3, 100))
		return heat < -ratio(60, 100) ? Biome::Snow : Biome::Beach;
	if (heat < -ratio(60, 100))
		return Biome::Snow;
	if (e > ratio(50, 100))
		return heat < -ratio(10, 100) ? Biome::Snow : Biome::Rock;
	if (heat < -ratio(30, 100))
		return wet > 0 ? Biome::Taiga : Biome::Tundra;
	if (heat > ratio(35, 100))
		return wet > ratio(15, 100) ? Biome::Jungle : wet < -ratio(15, 100) ? Biome::Desert : Biome::Savanna;
	return wet > ratio(5, 100) ? Biome::Forest : Biome::Grassland;
}

// The grid position of (u, v) between cell centres: cell indices and fractions.
struct GridPoint
{
	int32_t cx0, cx1, cz0, cz1;
	fix tx, tz;
};

static GridPoint gridPoint(fix u, fix v)
{
	fix gx = u * GRID - HALF, gz = v * GRID - HALF;
	int32_t ix = floorInt(gx), iz = floorInt(gz);
	GridPoint p;
	p.cx0 = (ix + GRID) % GRID;
	p.cx1 = (ix + 1 + GRID) % GRID;
	p.cz0 = std::clamp(iz, 0, GRID - 1);
	p.cz1 = std::clamp(iz + 1, 0, GRID - 1);
	p.tx = fraction(gx);
	p.tz = fraction(gz);
	return p;
}

void World::tectonic(fix u, fix v, fix &belt, fix &relief) const
{
	const Tectonics &t = m_grid->tectonics;
	GridPoint p = gridPoint(u, v);
	int32_t a = Grid::index(p.cx0, p.cz0), b = Grid::index(p.cx1, p.cz0), c = Grid::index(p.cx0, p.cz1), d = Grid::index(p.cx1, p.cz1);
	belt = lerp(lerp(t.belt[a], t.belt[b], p.tx), lerp(t.belt[c], t.belt[d], p.tx), p.tz);
	relief = lerp(lerp(t.relief[a], t.relief[b], p.tx), lerp(t.relief[c], t.relief[d], p.tx), p.tz);
}

int32_t World::cell(fix u, fix v) const
{
	int32_t cx = std::clamp(floorInt(u * GRID), 0, GRID - 1), cz = std::clamp(floorInt(v * GRID), 0, GRID - 1);
	return Grid::index(cx, cz);
}

void World::volcanic(int32_t x, int32_t z, int32_t &raise, int32_t &lava, int32_t &floor) const
{
	const Grid &g = *m_grid;
	raise = 0;
	lava = INT32_MIN;
	floor = INT32_MIN;
	int32_t i = cell(u(x), v(z));
	for (uint32_t k = g.volcano_start[i]; k < g.volcano_start[i + 1]; k++) {
		const Volcano &vo = g.volcanoes[g.volcano_list[k]];
		int32_t dx = x - vo.x;
		dx = ((dx + m_code.size / 2) % m_code.size + m_code.size) % m_code.size - m_code.size / 2; // east–west wrap
		int32_t dz = z - vo.z;
		int64_t d2 = (int64_t)dx * dx + (int64_t)dz * dz;
		if (d2 >= (int64_t)vo.radius * vo.radius)
			continue;
		fix d = sqrtWide64(d2 << FRAC); // blocks, fixed
		// the cone: (1 − d/r)^1.5, steeper near the top
		fix t = ONE - d / vo.radius;
		int32_t cone = floorInt(mul(toFix(vo.height), mul(t, sqrt(t))));
		if (d < toFix(vo.crater)) {
			// the crater: a bowl sunk below the rim
			fix r = div(d, toFix(vo.crater));
			fix rim_t = ONE - div(toFix(vo.crater), toFix(vo.radius));
			int32_t rim = floorInt(mul(toFix(vo.height), mul(rim_t, sqrt(rim_t))));
			int32_t depth = vo.crater * 3 / 4;
			// built from the vent's ground, so the rim is level and a lava lake sits flat in it
			floor = std::max(floor, vo.base + rim - floorInt(mul(toFix(depth), ONE - mul(r, r))));
			if (vo.active)
				lava = std::max(lava, vo.base + rim - depth / 2);
			continue;
		}
		raise = std::max(raise, cone);
	}
}

fix World::erosion(fix u, fix v) const
{
	const Grid &g = *m_grid;
	GridPoint p = gridPoint(u, v);
	auto worn = [&](int32_t cx, int32_t cz) {
		int32_t i = Grid::index(cx, cz);
		return g.eroded[i] - g.base[i];
	};
	return lerp(lerp(worn(p.cx0, p.cz0), worn(p.cx1, p.cz0), p.tx),
			lerp(worn(p.cx0, p.cz1), worn(p.cx1, p.cz1), p.tx), p.tz);
}

Column World::column(fix u, fix v, fix sun) const
{
	const WorldCode &c = m_code;
	fix belt, relief;
	tectonic(u, v, belt, relief);
	fix e = elevation(c, u, v, m_grid->sea_offset, belt, relief);
	if (e >= 0) // only the land wears down; the sea floor stays as made
		e = std::max<fix>(e + erosion(u, v), 0);
	fix heat = mul(sun - ratio(55, 100), ratio(22, 10)) + c.star->warmth + mul(c.temperature, ratio(6, 10))
			+ mul(fbm(u, v, c.seed_hash + HEAT, 3, 3, HALF), ratio(2, 10)) - mul(maxx(0, e), ratio(9, 10));
	fix wet = moisture(c, u, v);

	bool polar = v < POLAR_EDGE || v > ONE - POLAR_EDGE;
	Biome biome = polar ? (e < 0 ? Biome::SeaIce : Biome::Snow) : classify(e, heat, wet);
	return { e, heat, wet, biome, polar };
}

// °C for a heat, fitted to the biome thresholds in classify(): snow below
// about −10 °C, taiga and tundra below 0 °C, jungle and desert above 24 °C.
static fix celsius(fix heat)
{
	return toFix(11) + heat * 37;
}

// The daily swing either side of the day's mean, in °C: 6 where the air is
// average, 3 where soaked, 9 where arid; half that over water.
static fix dailySwing(const Column &c)
{
	fix swing = toFix(6) - c.wet * 3;
	return c.elevation < 0 ? swing / 2 : swing;
}

fix World::temperature(int32_t x, int32_t z, fix date, fix hour) const
{
	fix u = this->u(x), v = this->v(z);
	Column c = column(u, v, sunlight(v, date));
	// warmest at 15:00, coldest at 03:00
	fix daily = mul(dailySwing(c), cos(mul(TWO_PI, hour - ratio(5, 8))));
	return celsius(c.heat) + daily;
}

fix World::u(int32_t x) const
{
	int32_t half = m_code.size / 2;
	int32_t i = (x + half) % m_code.size;
	if (i < 0)
		i += m_code.size;
	return ratio(i, m_code.size);
}

fix World::v(int32_t z) const
{
	// +z is north; north is v = 0
	return (fix)(((int64_t)(m_code.size / 2 - z) * ONE) / m_code.size);
}

int32_t World::surface(fix e, fix u, fix v) const
{
	fix scale = e >= 0 ? toFix(LAND_SCALE) : toFix(SEA_SCALE);
	fix detail = fbm(u, v, m_code.seed_hash + DETAIL, m_code.size / 64, 3, HALF);
	fix amplitude = e >= 0 ? toFix(5) : toFix(2);
	return floorInt(mul(e, scale) + mul(detail, amplitude));
}

// A water surface in blocks: lakes and rivers sit at the grid's filled level.
static int32_t waterBlocks(fix filled)
{
	return std::max<int32_t>(0, floorInt(filled * LAND_SCALE));
}

// Rivers: segments from each river cell's centre to the centre of the cell it
// drains into. These say how wide, and how high the banks stand over the water.
static const int32_t BANK = 6;   // blocks beside the channel kept above the water
static const fix MAX_WIDTH = toFix(24);

Ground World::ground(int32_t x, int32_t z, fix sun) const
{
	const Grid &g = *m_grid;
	fix u = this->u(x), v = this->v(z);
	Column c = column(u, v, sun);
	Ground out{ surface(c.elevation, u, v), 0, Water::None, c };
	// volcanoes stand on the land (or rise from the sea floor to islands)
	int32_t raise, lava, floor;
	volcanic(x, z, raise, lava, floor);
	if (raise > 0)
		out.terrain += raise;
	if (floor != INT32_MIN) {
		out.terrain = std::max(out.terrain, floor);
		if (lava != INT32_MIN && lava > out.terrain && lava > 1) {
			out.water = lava; // an active crater: a lava lake
			out.kind = Water::Lava;
			return out;
		}
	}
	if (out.terrain < 0) {
		out.kind = Water::Sea;
		out.water = 0;
	}

	// Lakes: the grid's lake mask, between cell centres. Inside the outline the
	// floor drops under the lake's level; on the rim the ground holds it in.
	GridPoint p = gridPoint(u, v);
	int32_t corners[4] = {
		Grid::index(p.cx0, p.cz0), Grid::index(p.cx1, p.cz0),
		Grid::index(p.cx0, p.cz1), Grid::index(p.cx1, p.cz1),
	};
	fix mask = lerp(lerp(toFix(g.lake[corners[0]]), toFix(g.lake[corners[1]]), p.tx),
			lerp(toFix(g.lake[corners[2]]), toFix(g.lake[corners[3]]), p.tx), p.tz);
	if (mask > 0) {
		int32_t level = 0;
		for (int32_t i : corners)
			if (g.lake[i])
				level = std::max(level, waterBlocks(g.lake_level[i]));
		if (mask >= HALF) {
			int32_t depth = 1 + roundInt(mul(mask - HALF, toFix(6)));
			out.terrain = std::min(out.terrain, level - depth);
			out.water = level;
			out.kind = Water::Lake;
		} else {
			out.terrain = std::max(out.terrain, level);
		}
	}

	// Rivers: the nearest river segment within reach of this column.
	int32_t cell = m_code.size / GRID;
	int32_t half_size = m_code.size / 2;
	int32_t ix = std::clamp(floorInt(v * GRID), 0, GRID - 1);
	int32_t jx = floorInt(u * GRID);
	fix best_gap = toFix(1 << 14);
	int32_t best_level = 0;
	fix best_half = 0, best_dist = 0;
	for (int32_t dz = -2; dz <= 2; dz++) {
		int32_t cz = ix + dz;
		if (cz < 0 || cz >= GRID)
			continue;
		for (int32_t dx = -2; dx <= 2; dx++) {
			int32_t cx = (jx + dx + GRID) % GRID;
			int32_t i = Grid::index(cx, cz);
			int32_t r = g.receiver[i];
			if (!g.river(i) || r < 0)
				continue;
			// segment ends in block coordinates, relative to this column
			auto centre = [&](int32_t k, int32_t &bx, int32_t &bz) {
				int32_t kx = k % GRID, kz = k / GRID;
				bx = -half_size + kx * cell + cell / 2 - x;
				bz = half_size - (kz * cell + cell / 2) - z;
				bx = ((bx + half_size) % m_code.size + m_code.size) % m_code.size - half_size; // east–west wrap
			};
			int32_t ax, az, bx, bz;
			centre(i, ax, az);
			centre(r, bx, bz);
			int64_t sx = bx - ax, sz = bz - az;
			int64_t len2 = sx * sx + sz * sz;
			if (len2 == 0)
				continue;
			// closest point on the segment to the origin (this column)
			fix t = (fix)std::clamp<int64_t>(((-ax * sx - az * sz) << FRAC) / len2, 0, ONE);
			int64_t px = ((int64_t)ax << FRAC) + sx * t, pz = ((int64_t)az << FRAC) + sz * t; // fixed blocks
			fix dist = sqrtWide64(px * px + pz * pz);
			// wider as more water passes: 3 blocks at the threshold, √ of the flow beyond it
			fix width = std::min(toFix(3) + 2 * sqrtWide64((g.drained[i] << FRAC) / g.river_threshold), MAX_WIDTH);
			fix half = width / 2;
			if (dist - half < best_gap) {
				best_gap = dist - half;
				best_half = half;
				best_dist = dist;
				int32_t from = waterBlocks(g.filled[i]) - 1;
				int32_t to = g.base[r] < 0 ? 0 : waterBlocks(g.filled[r]) - 1;
				best_level = std::max<int32_t>(0, floorInt(lerp(toFix(from), toFix(to), t)));
			}
		}
	}
	if (best_gap < toFix(BANK)) {
		if (best_dist < best_half) {
			// the channel: a bed under the water, deepest mid-stream
			int32_t depth = 1 + std::min(3, roundInt(mul(best_half - best_dist, HALF)));
			out.terrain = std::min(out.terrain, best_level - depth);
			if (out.kind == Water::None || best_level > out.water) {
				out.water = best_level;
				out.kind = Water::River;
			}
		} else if (out.terrain < best_level + 1) {
			out.terrain = best_level + 1; // the bank holds the river in
		}
	}
	return out;
}

} // namespace terragen
