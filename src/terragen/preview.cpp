// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "preview.h"
#include <algorithm>

namespace terragen {

template <typename T>
static T quantize(fix v, int32_t scale, int32_t lo, int32_t hi)
{
	int64_t q = ((int64_t)v * scale) >> FRAC;
	return (T)(q < lo ? lo : q > hi ? hi : q);
}

Preview preview(const World &world, std::optional<fix> date, int32_t resolution)
{
	Preview p;
	p.resolution = resolution;
	size_t n = (size_t)resolution * resolution;
	p.elevation.resize(n);
	p.heat.resize(n);
	p.wet.resize(n);
	p.season.resize(n);
	p.fertility.resize(n);
	p.current.resize(n);
	p.biome.resize(n);
	p.lake.resize(n);
	p.province.resize(n);
	p.plate.resize(n);
	p.boundary.resize(n);
	const Grid &g = world.grid();

	for (int32_t y = 0; y < resolution; y++) {
		fix v = ratio(2 * y + 1, 2 * resolution); // cell centres
		fix sun = world.sunlight(v, date);
		for (int32_t x = 0; x < resolution; x++) {
			Column c = world.column(ratio(x, resolution), v, sun);
			size_t i = (size_t)y * resolution + x;
			p.elevation[i] = quantize<int16_t>(c.elevation, 10000, -32767, 32767);
			p.heat[i] = quantize<int8_t>(c.heat, 100, -127, 127);
			p.wet[i] = quantize<int8_t>(c.wet, 100, -127, 127);
			p.season[i] = quantize<int8_t>(c.season, 100, -127, 127);
			p.biome[i] = (uint8_t)c.biome;
			int32_t cell = Grid::index(std::min(GRID - 1, x * GRID / resolution), std::min(GRID - 1, y * GRID / resolution));
			p.lake[i] = g.lake[cell];
			p.current[i] = quantize<int8_t>(world.between(g.sea_heat, ratio(x, resolution), v), 100, -127, 127);
			p.fertility[i] = c.elevation < 0 ? 0 : (uint8_t)std::clamp(roundInt(g.fertility[cell] * 255), 0, 255);
			p.province[i] = (uint8_t)g.province[cell];
			p.plate[i] = g.tectonics.plate[cell];
			p.boundary[i] = g.tectonics.distance[cell] == 0 ? (uint8_t)g.tectonics.boundary[cell] : 0;
		}
	}
	p.grid = GRID;
	for (int32_t i = 0; i < GRID * GRID; i++) {
		int32_t r = g.receiver[i];
		if (!g.river(i) || r < 0)
			continue;
		int32_t dx = r % GRID - i % GRID, dz = r / GRID - i / GRID;
		if (dx > 1) dx -= GRID; // across the east–west seam
		if (dx < -1) dx += GRID;
		int32_t strength = roundInt(sqrtWide64((g.drained[i] << FRAC) / g.river_threshold) * 16);
		p.rivers.push_back((uint8_t)(i % GRID));
		p.rivers.push_back((uint8_t)(i / GRID));
		p.rivers.push_back((uint8_t)(int8_t)dx);
		p.rivers.push_back((uint8_t)(int8_t)dz);
		p.rivers.push_back((uint8_t)std::clamp(strength, 1, 255));
	}
	int32_t size = world.code().size, half = size / 2;
	for (const Volcano &v : g.volcanoes) {
		uint32_t u = (uint32_t)(((int64_t)(v.x + half) * 65535) / size), vv = (uint32_t)(((int64_t)(half - v.z) * 65535) / size);
		p.volcanoes.push_back(u & 255);
		p.volcanoes.push_back(u >> 8);
		p.volcanoes.push_back(vv & 255);
		p.volcanoes.push_back(vv >> 8);
		p.volcanoes.push_back(v.active ? 1 : 0);
	}
	return p;
}

} // namespace terragen
