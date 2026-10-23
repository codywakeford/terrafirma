// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "weather.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>

namespace weather {

using namespace terragen;

static const int32_t CELLS = 64;          // the layer's grid, per side
static const int32_t SPAN = GRID / CELLS;  // terragen grid cells per layer cell
static const double STEP = 3 * 3600;       // s
static const double DAY = 86400;
static const double LAND_METRES = 4000;    // a unit of terragen elevation, in the air's metres
static const char MAGIC[8] = { 'T', 'F', 'W', 'S', 0, 0, 0, 1 };

double clock(uint32_t day_count, double time_of_day)
{
	return (day_count + time_of_day) * DAY;
}

static double toDouble(fix v)
{
	return (double)v / ONE;
}

// Earth's fluid layer for a terra world.
static fluid::LayerOpts earth(const World &world, const matter::Matter &m)
{
	const WorldCode &code = world.code();
	const Grid &grid = world.grid();
	fluid::LayerOpts o;
	// sunlight stays Earth's: the star's warmth is already in terragen's
	// climate, which anchors the layer
	o.planet = { 6.371e6, 9.81, DAY, toDouble(code.tilt) * M_PI / 180, code.year * DAY, 1361 };
	o.width = o.height = CELLS;
	o.seed = code.seed_hash;

	double weighted = 0, weights = 0;
	int32_t half = code.size / 2;
	for (int32_t row = 0; row < CELLS; row++) {
		for (int32_t col = 0; col < CELLS; col++) {
			double height = 0, wet = 0;
			int sea = 0;
			for (int32_t dz = 0; dz < SPAN; dz++)
				for (int32_t dx = 0; dx < SPAN; dx++) {
					int32_t i = Grid::index(col * SPAN + dx, row * SPAN + dz);
					height += std::max(toDouble(grid.eroded[i]), 0.0) * LAND_METRES;
					sea += grid.base[i] < 0 || grid.lake[i];
				}
			height /= SPAN * SPAN;
			bool water = sea * 2 > SPAN * SPAN;
			// the cell's centre, held inside the polar edge
			fix u = ratio(2 * col + 1, 2 * CELLS);
			fix v = clamp(ratio(2 * row + 1, 2 * CELLS), POLAR_EDGE + ratio(1, 256), ONE - POLAR_EDGE - ratio(1, 256));
			int32_t x = (int32_t)((int64_t)u * code.size / ONE) - half;
			int32_t z = half - (int32_t)((int64_t)v * code.size / ONE);
			wet = toDouble(world.column(u, v, world.sunlight(v, std::nullopt)).wet);
			// the annual mean: four seasons, at the hour the daily swing crosses zero
			double anchor = 0;
			for (int k = 0; k < 4; k++)
				anchor += toDouble(world.temperature(x, z, ratio(k, 4), ratio(3, 8))) / 4;
			anchor += 273.15;
			o.ground.push_back({ water ? -1.0 : height, water, std::clamp((wet + 1) / 2, 0.0, 1.0), anchor });
			double area = std::cos(M_PI / 2 - (row + 0.5) * M_PI / CELLS);
			weighted += anchor * area;
			weights += area;
		}
	}
	o.air = { { { m.substance("nitrogen"), 0.79 }, { m.substance("oxygen"), 0.21 } }, 101325, m.substance("water"),
		weighted / weights };
	for (const Volcano &vo : grid.volcanoes) {
		if (!vo.active)
			continue;
		int32_t col = std::clamp((int32_t)((int64_t)(vo.x + half) * CELLS / code.size), 0, CELLS - 1);
		int32_t row = std::clamp((int32_t)((int64_t)(half - vo.z) * CELLS / code.size), 0, CELLS - 1);
		o.vents.push_back({ row * CELLS + col, 0.02 });
	}
	return o;
}

static double dateOf(double t, double year)
{
	double d = std::fmod(t / year, 1.0);
	return d < 0 ? d + 1 : d;
}

Weather::Weather(const Opts &opts) :
	m_world(opts.world), m_path(opts.path), m_layer_opts(earth(opts.world, opts.matter))
{
}

std::unique_ptr<Weather> Weather::open(const Opts &opts, double now)
{
	std::unique_ptr<Weather> w(new Weather(opts));
	std::ifstream in(opts.path, std::ios::binary);
	if (opts.path.empty() || !in) {
		w->settle(now);
		return w;
	}
	std::stringstream bytes;
	bytes << in.rdbuf();
	std::string s = bytes.str();
	if (s.size() < sizeof MAGIC + sizeof(double) || s.compare(0, sizeof MAGIC, MAGIC, sizeof MAGIC) != 0)
		throw fluid::FluidError("weather: " + opts.path + " isn't a weather file");
	std::memcpy(&w->m_start, s.data() + sizeof MAGIC, sizeof(double));
	w->m_layer = std::make_unique<fluid::Layer>(w->m_layer_opts);
	w->m_layer->load(s.substr(sizeof MAGIC + sizeof(double)));
	w->advance(now);
	return w;
}

void Weather::settle(double now)
{
	double year = m_layer_opts.planet.year;
	m_layer = std::make_unique<fluid::Layer>(m_layer_opts);
	m_start = now - year;
	advance(now);
}

void Weather::advance(double target)
{
	double year = m_layer_opts.planet.year;
	if (target < now() - STEP || target > now() + year) {
		settle(target);
		return;
	}
	bool new_day = false;
	while (now() + STEP <= target) {
		double before = now();
		m_layer->step(STEP, dateOf(before, year));
		new_day = new_day || std::floor(before / DAY) != std::floor(now() / DAY);
	}
	if (new_day)
		save();
}

fluid::Sample Weather::sample(int32_t x, int32_t y, int32_t z, double now) const
{
	double u = toDouble(m_world.u(x)), v = std::clamp(toDouble(m_world.v(z)), 0.0, 1.0 - 1e-9);
	return m_layer->sample(u, v, y * BLOCK_METRES, std::fmod(now / DAY, 1.0));
}

void Weather::save() const
{
	if (m_path.empty())
		return;
	std::string tmp = m_path + ".tmp";
	{
		std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
		out.write(MAGIC, sizeof MAGIC);
		out.write(reinterpret_cast<const char *>(&m_start), sizeof m_start);
		std::string state = m_layer->save();
		out.write(state.data(), state.size());
		if (!out)
			throw fluid::FluidError("weather: couldn't write " + tmp);
	}
	if (std::rename(tmp.c_str(), m_path.c_str()) != 0)
		throw fluid::FluidError("weather: couldn't replace " + m_path);
}

std::vector<fluid::Layer::Fields> Weather::year(const World &world, const matter::Matter &matter, int snapshots)
{
	Weather w(Opts{ world, matter, "" });
	double year = w.m_layer_opts.planet.year;
	w.settle(0);
	std::vector<fluid::Layer::Fields> out;
	for (int k = 0; k < snapshots; k++) {
		w.advance(k * year / snapshots);
		out.push_back(w.m_layer->fields());
	}
	return out;
}

} // namespace weather
