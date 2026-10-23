// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "geology.h"
#include "noise.h"
#include "world.h"
#include <algorithm>

namespace terragen {

namespace {

enum VegStream : uint32_t { WOODS = 211, SPOT = 223, ROLL = 227 };

uint32_t mix(uint32_t a, uint32_t b, uint32_t seed)
{
	uint32_t h = seed ^ (a * 0x27d4eb2du) ^ (b * 0x165667b1u);
	h = (h ^ (h >> 15)) * 0x2c1b3c6du;
	h = (h ^ (h >> 12)) * 0x297a2d39u;
	return h ^ (h >> 15);
}

// 0 below lo, 1 above hi, a straight ramp between
fix ramp(fix v, fix lo, fix hi)
{
	return clamp(div(v - lo, hi - lo), 0, ONE);
}

const int32_t SPOT_CELL = 5; // blocks: at most one tree per 5×5

} // namespace

Cover World::cover(int32_t x, int32_t z, const Ground &ground) const
{
	const Grid &g = *m_grid;
	fix u = this->u(x), v = this->v(z);
	const Column &c = ground.column;
	Cover out{ 0, 0, false, false, mix((uint32_t)x, (uint32_t)z, m_code.seed_hash + ROLL) };
	if (ground.kind != Water::None || c.polar || c.elevation < 0)
		return out;

	// the lie of the land, from the coarse grid's gradient
	int32_t i = cell(u, v), cx = i % GRID, cz = i / GRID;
	auto at = [&](int32_t dx, int32_t dz) {
		int32_t zz = std::clamp(cz + dz, 0, GRID - 1);
		return g.eroded[Grid::index(((cx + dx) % GRID + GRID) % GRID, zz)];
	};
	int32_t cell_blocks = m_code.size / GRID;
	fix gx = (at(1, 0) - at(-1, 0)) * LAND_SCALE / (2 * cell_blocks);
	fix gz = (at(0, 1) - at(0, -1)) * LAND_SCALE / (2 * cell_blocks);
	out.slope = sqrt(mul(gx, gx) + mul(gz, gz));

	// beside water: a river or lake within a cell
	for (int32_t dz = -1; dz <= 1 && !out.bank; dz++)
		for (int32_t dx = -1; dx <= 1 && !out.bank; dx++) {
			int32_t zz = cz + dz;
			if (zz < 0 || zz >= GRID)
				continue;
			int32_t j = Grid::index(((cx + dx) % GRID + GRID) % GRID, zz);
			out.bank = g.river(j) || g.lake[j];
		}

	// warmth (heat already falls with height: treelines emerge) and water
	fix warm = ramp(c.heat, -ratio(45, 100), -ratio(15, 100));
	fix wet = ramp(c.wet, -ratio(40, 100), ratio(30, 100));
	if (out.bank) // rivers water their banks through dry country
		wet = std::max(wet, ratio(75, 100));
	fix density = mul(warm, wet);
	// steep ground holds little soil; bare peaks none
	density = mul(density, ONE - ramp(out.slope, ratio(8, 10), ratio(16, 10)));
	if (c.elevation > ratio(55, 100))
		density = 0;
	if (g.province[i] == Province::Volcanic && density > 0) // ash makes rich soil
		density = std::min<fix>(ONE, density + ratio(10, 100));
	// woods and clearings
	fix woods = value(u, v, m_code.seed_hash + WOODS, std::max(8, m_code.size / 160));
	density = clamp(mul(density, ratio(70, 100) + mul(woods, ratio(60, 100))), 0, ONE);
	out.density = density;

	// a tree here? one jittered spot per small cell, kept if the roll is under the density
	int32_t sx = (x >= 0 ? x : x - SPOT_CELL + 1) / SPOT_CELL, sz = (z >= 0 ? z : z - SPOT_CELL + 1) / SPOT_CELL;
	uint32_t h = mix((uint32_t)sx, (uint32_t)sz, m_code.seed_hash + SPOT);
	int32_t px = sx * SPOT_CELL + (int32_t)(h % SPOT_CELL), pz = sz * SPOT_CELL + (int32_t)((h >> 8) % SPOT_CELL);
	out.tree = px == x && pz == z && (fix)((h >> 12) & (ONE - 1)) < mul(density, ratio(85, 100));
	return out;
}

} // namespace terragen
