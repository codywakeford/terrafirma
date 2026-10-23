// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "noise.h"

namespace terragen {

static uint32_t hash(int32_t x, int32_t y, uint32_t seed)
{
	uint32_t h = seed ^ ((uint32_t)x * 0x27d4eb2du) ^ ((uint32_t)y * 0x165667b1u);
	h = (h ^ (h >> 15)) * 0x2c1b3c6du;
	h = (h ^ (h >> 12)) * 0x297a2d39u;
	return h ^ (h >> 15);
}

// A lattice value in [−1, 1)
static fix lattice(int32_t x, int32_t y, uint32_t seed)
{
	return (fix)((hash(x, y, seed) >> 15) & 0x1FFFF) - ONE;
}

static int32_t wrap(int32_t i, int32_t period)
{
	i %= period;
	return i < 0 ? i + period : i;
}

fix value(fix u, fix v, uint32_t seed, int32_t frequency)
{
	fix x = u * frequency, y = v * frequency;
	int32_t ix = floorInt(x), iy = floorInt(y);
	fix tx = smooth(fraction(x)), ty = smooth(fraction(y));
	int32_t x0 = wrap(ix, frequency), x1 = wrap(ix + 1, frequency);
	fix top = lerp(lattice(x0, iy, seed), lattice(x1, iy, seed), tx);
	fix bottom = lerp(lattice(x0, iy + 1, seed), lattice(x1, iy + 1, seed), tx);
	return lerp(top, bottom, ty);
}

fix fbm(fix u, fix v, uint32_t seed, int32_t frequency, int octaves, fix persistence)
{
	fix sum = 0, amp = ONE, norm = 0;
	for (int o = 0; o < octaves; o++) {
		sum += mul(amp, value(u, v, seed + (uint32_t)o * 1013u, frequency));
		norm += amp;
		amp = mul(amp, persistence);
		frequency *= 2;
	}
	return div(sum, norm);
}

static uint32_t hash3(int32_t x, int32_t y, int32_t z, uint32_t seed)
{
	return hash(x, y, seed ^ ((uint32_t)z * 0x9e3779b1u));
}

static fix lattice3(int32_t x, int32_t y, int32_t z, uint32_t seed)
{
	return (fix)((hash3(x, y, z, seed) >> 15) & 0x1FFFF) - ONE;
}

// floor division, and the fraction as a fixed value
static int32_t cellOf(int32_t v, int32_t cell, fix &t)
{
	int32_t i = v >= 0 ? v / cell : -((-v + cell - 1) / cell);
	t = (fix)(((int64_t)(v - i * cell) << FRAC) / cell);
	return i;
}

fix value3(int32_t x, int32_t y, int32_t z, uint32_t seed, int32_t cell, int32_t period)
{
	fix tx, ty, tz;
	int32_t ix = cellOf(x, cell, tx), iy = cellOf(y, cell, ty), iz = cellOf(z, cell, tz);
	int32_t cells = period / cell;
	int32_t x0 = wrap(ix, cells), x1 = wrap(ix + 1, cells);
	tx = smooth(tx);
	ty = smooth(ty);
	tz = smooth(tz);
	auto plane = [&](int32_t yy) {
		fix a = lerp(lattice3(x0, yy, iz, seed), lattice3(x1, yy, iz, seed), tx);
		fix b = lerp(lattice3(x0, yy, iz + 1, seed), lattice3(x1, yy, iz + 1, seed), tx);
		return lerp(a, b, tz);
	};
	return lerp(plane(iy), plane(iy + 1), ty);
}

} // namespace terragen
