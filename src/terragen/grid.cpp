// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "grid.h"
#include "noise.h"
#include "world.h"
#include <algorithm>
#include <map>
#include <mutex>
#include <queue>

namespace terragen {

static int octavesFor(const WorldCode &c) { return 3 + roundInt(c.roughness * 3); }

fix landmass(const WorldCode &c, fix u, fix v)
{
	fix persistence = ratio(35, 100) + mul(c.roughness, ratio(25, 100));
	// continents, with the coastline broken up by finer noise
	return fbm(u, v, c.seed_hash + CONTINENT, c.continents, octavesFor(c), persistence)
			+ mul(fbm(u, v, c.seed_hash + COAST, c.continents * 7, 3, HALF), mul(c.coast, ratio(22, 100)));
}

fix elevation(const WorldCode &c, fix u, fix v, fix sea_offset, fix belt, fix relief)
{
	fix land = landmass(c, u, v) - sea_offset;
	if (land < 0) // ridges and trenches shape the sea floor, never above the sea
		return std::min<fix>(land + relief, -ratio(1, 160));
	// mountain ridges, sharp where their noise crosses zero, rising from the coast inland
	fix ridge = ONE - fabsx(fbm(u, v, c.seed_hash + RIDGE, c.continents * 4, octavesFor(c), HALF));
	fix inland = clamp(div(land, ratio(25, 100)), 0, ONE);
	// plains level off inland (land / (land + 0.3) · 0.4 → at most ~0.4):
	// height comes from mountain ranges, not from distance to the coast
	fix plains = div(mul(land, ratio(4, 10)), land + ratio(3, 10));
	// ranges run along the plates' collisions: the belt scales them (×1.6 at
	// a boundary, low in plate interiors); rifts sink, never below the sea
	fix range = mul(mul(mul(mul(ridge, ridge), c.mountains), inland), mul(belt, ratio(16, 10)));
	return std::max<fix>(plains + range + relief, ratio(1, 160));
}

fix moisture(const WorldCode &c, fix u, fix v)
{
	return fbm(u, v, c.seed_hash + WET, 3, 3, HALF) + mul(c.humidity, ratio(6, 10));
}

namespace {

// Erosion: how hard running water bites, and how many rounds it gets.
const fix ERODIBILITY = ratio(6, 100);
const int EROSION_ROUNDS = 12;
// Water deeper than a block (at 160 blocks per unit) counts as a lake…
const fix LAKE_DEPTH = ratio(1, 160);
// …if its basin is big and deep enough; smaller hollows fill with sediment,
// and so do basins wider than this share of the land (they'd be inland seas).
const int32_t LAKE_MIN_CELLS = 10;
const fix LAKE_MIN_DEEPEST = ratio(6, 160);
const fix LAKE_MAX_SHARE = ratio(3, 100);
// Steepness toward a neighbour is drop ÷ distance; compared as drop × weight
// (1000 straight, 707 ≈ 1000/√2 diagonal) so a one-unit drop never rounds away.
const int64_t STRAIGHT_WEIGHT = 1000, DIAGONAL_WEIGHT = 707;

const int DX[8] = { -1, 0, 1, -1, 1, -1, 0, 1 };
const int DZ[8] = { -1, -1, -1, 0, 0, 1, 1, 1 };

int32_t neighbour(int32_t i, int k)
{
	int32_t cx = i % GRID + DX[k], cz = i / GRID + DZ[k];
	if (cz < 0 || cz >= GRID)
		return -1;
	cx = (cx + GRID) % GRID; // east–west wraps
	return Grid::index(cx, cz);
}

fix jitter(int32_t i)
{
	uint32_t h = (uint32_t)i * 0x9e3779b1u;
	h ^= h >> 15;
	h *= 0x85ebca77u;
	return (fix)((h >> 16) & 63);
}

/*
	Priority-flood (Barnes, Lehman & Mulla 2014), with an epsilon so every
	land cell ends strictly above the cell that reached it: water can always
	run downhill to the sea. Seeds are the sea cells; with no sea at all, the
	lowest cell. Ties pop by index, so the result is deterministic.
*/
std::vector<fix> fill(const Grid &g, const std::vector<fix> &h)
{
	using Entry = std::pair<fix, int32_t>;
	std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> open;
	std::vector<fix> out = h;
	std::vector<uint8_t> seen(h.size(), 0);

	int32_t lowest = -1;
	for (int32_t i = 0; i < (int32_t)h.size(); i++) {
		if (g.polar[i])
			continue;
		if (lowest < 0 || h[i] < h[lowest])
			lowest = i;
		if (h[i] < 0) {
			seen[i] = 1;
			open.push({ h[i], i });
		}
	}
	if (open.empty() && lowest >= 0) {
		seen[lowest] = 1;
		open.push({ h[lowest], lowest });
	}

	while (!open.empty()) {
		auto [level, c] = open.top();
		open.pop();
		for (int k = 0; k < 8; k++) {
			int32_t n = neighbour(c, k);
			if (n < 0 || seen[n] || g.polar[n])
				continue;
			seen[n] = 1;
			// the step up is jittered per cell (under a fifth of a block), so
			// water crossing a filled flat meanders instead of running ruler-straight
			out[n] = std::max(h[n], level + 1 + jitter(n));
			open.push({ out[n], n });
		}
	}
	return out;
}

// Where each cell drains: its steepest strictly-lower neighbour on `surface`;
// −1 for the sea, polar walls, and (on an unfilled surface) pits.
std::vector<int32_t> receivers(const Grid &g, const std::vector<fix> &surface)
{
	std::vector<int32_t> out(surface.size(), -1);
	for (int32_t i = 0; i < (int32_t)surface.size(); i++) {
		if (g.polar[i] || surface[i] < 0)
			continue;
		int64_t best = 0;
		for (int k = 0; k < 8; k++) {
			int32_t n = neighbour(i, k);
			if (n < 0 || g.polar[n])
				continue;
			int64_t steep = (int64_t)(surface[i] - surface[n]) * (DX[k] != 0 && DZ[k] != 0 ? DIAGONAL_WEIGHT : STRAIGHT_WEIGHT);
			if (steep > best) {
				best = steep;
				out[i] = n;
			}
		}
	}
	return out;
}

// Cells ordered so every cell comes after the cell it drains into: from each
// outlet upstream, breadth first, outlets in index order.
std::vector<int32_t> upstreamOrder(const std::vector<int32_t> &receiver)
{
	size_t n = receiver.size();
	std::vector<int32_t> start(n + 1, 0), donors(n);
	for (int32_t r : receiver)
		if (r >= 0)
			start[r + 1]++;
	for (size_t i = 0; i < n; i++)
		start[i + 1] += start[i];
	std::vector<int32_t> fillpos(start.begin(), start.end() - 1);
	for (int32_t i = 0; i < (int32_t)n; i++)
		if (receiver[i] >= 0)
			donors[fillpos[receiver[i]]++] = i;

	std::vector<int32_t> order;
	order.reserve(n);
	for (int32_t i = 0; i < (int32_t)n; i++)
		if (receiver[i] < 0)
			order.push_back(i);
	for (size_t head = 0; head < order.size(); head++) {
		int32_t c = order[head];
		for (int32_t d = start[c]; d < start[c + 1]; d++)
			order.push_back(donors[d]);
	}
	return order;
}

// Rain-weighted area draining through each cell.
std::vector<int64_t> accumulate(const std::vector<int32_t> &receiver, const std::vector<int32_t> &order,
		const std::vector<fix> &rain)
{
	std::vector<int64_t> area(receiver.size());
	for (size_t i = 0; i < area.size(); i++)
		area[i] = rain[i];
	for (size_t k = order.size(); k-- > 0;) {
		int32_t c = order[k];
		if (receiver[c] >= 0)
			area[receiver[c]] += area[c];
	}
	return area;
}

/*
	Water standing over the eroded land, grouped into basins (8-connected,
	wrapping east–west). A basin becomes a lake only if it is big and deep
	enough; smaller hollows fill with sediment — the land rises to the water.
*/
void keepRealLakes(Grid &g)
{
	size_t n = g.filled.size();
	int64_t land = 0;
	for (size_t i = 0; i < n; i++)
		land += !g.polar[i] && g.base[i] >= 0;
	int32_t max_cells = (int32_t)((land * LAKE_MAX_SHARE) >> FRAC);
	std::vector<uint8_t> wet(n), seen(n);
	for (size_t i = 0; i < n; i++)
		wet[i] = !g.polar[i] && g.filled[i] >= 0 && g.filled[i] - g.eroded[i] >= LAKE_DEPTH;

	std::vector<int32_t> basin;
	for (int32_t start = 0; start < (int32_t)n; start++) {
		if (!wet[start] || seen[start])
			continue;
		basin.clear();
		basin.push_back(start);
		seen[start] = 1;
		fix deepest = 0;
		for (size_t head = 0; head < basin.size(); head++) {
			int32_t c = basin[head];
			deepest = std::max(deepest, g.filled[c] - g.eroded[c]);
			for (int k = 0; k < 8; k++) {
				int32_t nb = neighbour(c, k);
				if (nb >= 0 && wet[nb] && !seen[nb]) {
					seen[nb] = 1;
					basin.push_back(nb);
				}
			}
		}
		bool real = (int32_t)basin.size() >= LAKE_MIN_CELLS && (int32_t)basin.size() <= max_cells &&
				deepest >= LAKE_MIN_DEEPEST;
		fix outlet = g.filled[start];
		for (int32_t c : basin)
			outlet = std::min(outlet, g.filled[c]);
		for (int32_t c : basin) {
			if (real) {
				g.lake[c] = 1;
				g.lake_level[c] = outlet;
			} else {
				g.eroded[c] = g.filled[c]; // sediment fills the hollow
			}
		}
	}
}

std::shared_ptr<Grid> build(const WorldCode &code)
{
	auto g = std::make_shared<Grid>();
	size_t n = (size_t)GRID * GRID;
	std::vector<fix> mass(n), rain(n);
	g->polar.resize(n);

	for (int32_t cz = 0; cz < GRID; cz++) {
		fix v = ratio(2 * cz + 1, 2 * GRID);
		for (int32_t cx = 0; cx < GRID; cx++) {
			fix u = ratio(2 * cx + 1, 2 * GRID);
			int32_t i = Grid::index(cx, cz);
			mass[i] = landmass(code, u, v);
			// rain where it's wet; a little everywhere so every slope runs
			rain[i] = clamp(ONE + moisture(code, u, v), ratio(2, 10), toFix(2));
			g->polar[i] = v < POLAR_EDGE || v > ONE - POLAR_EDGE;
		}
	}

	// The sea, by quantile of the landmass field: exactly the Land share of
	// the open (non-polar) cells is above it.
	std::vector<fix> open_cells;
	for (size_t i = 0; i < n; i++)
		if (!g->polar[i])
			open_cells.push_back(mass[i]);
	std::sort(open_cells.begin(), open_cells.end());
	size_t sea_cells = (size_t)(((int64_t)open_cells.size() * (ONE - code.land)) >> FRAC);
	sea_cells = std::min(sea_cells, open_cells.size() - 1);
	g->sea_offset = open_cells[sea_cells];
	// the plates, before the height: mountains follow their collisions
	g->tectonics = tectonics(code, mass, g->sea_offset);
	g->base.resize(n);
	for (int32_t cz = 0; cz < GRID; cz++)
		for (int32_t cx = 0; cx < GRID; cx++) {
			int32_t i = Grid::index(cx, cz);
			g->base[i] = elevation(code, ratio(2 * cx + 1, 2 * GRID), ratio(2 * cz + 1, 2 * GRID), g->sea_offset,
					g->tectonics.belt[i], g->tectonics.relief[i]);
		}

	// Erosion, implicit stream power (Braun & Willett 2013), n = 1: from each
	// outlet upstream, h ← (h + F·h_down) / (1 + F), F = K·√area. Lakes don't
	// erode their beds; the sea stays put.
	std::vector<fix> h = g->base;
	for (int round = 0; round < EROSION_ROUNDS; round++) {
		std::vector<fix> surface = fill(*g, h);
		std::vector<int32_t> down = receivers(*g, surface);
		std::vector<int32_t> order = upstreamOrder(down);
		std::vector<int64_t> area = accumulate(down, order, rain);
		for (int32_t c : order) {
			int32_t r = down[c];
			if (r < 0 || surface[c] > h[c])
				continue;
			fix F = mul(ERODIBILITY, sqrtWide64(area[c]));
			// the receiver's surface (its bed, or its lake); land never wears below the sea
			fix floor = std::max<fix>(surface[r], 0);
			fix next = div(h[c] + mul(F, floor), ONE + F);
			h[c] = std::max(next, floor + 1);
		}
	}
	g->eroded = h;

	// The final water: fill, keep the basins that make real lakes, drain, accumulate.
	g->filled = fill(*g, g->eroded);
	g->lake.resize(n);
	g->lake_level.resize(n);
	keepRealLakes(*g);
	g->receiver = receivers(*g, g->filled);
	g->drained = accumulate(g->receiver, upstreamOrder(g->receiver), rain);

	// Rivers: the Rivers setting picks what share of the land carries one
	// (0.4% to 4%), so every world gets its proportion, whatever its shape.
	std::vector<int64_t> land_drained;
	for (size_t i = 0; i < n; i++)
		if (!g->polar[i] && g->base[i] >= 0)
			land_drained.push_back(g->drained[i]);
	std::sort(land_drained.begin(), land_drained.end());
	fix share = ratio(4, 1000) + mul(code.rivers, ratio(36, 1000));
	size_t at = land_drained.size() - 1 - std::min(land_drained.size() - 1, (size_t)(((int64_t)land_drained.size() * share) >> FRAC));
	g->river_threshold = land_drained.empty() ? INT64_MAX : std::max<int64_t>(land_drained[at], toFix(8));

	// geology on the finished land: volcanoes, which cells their slopes
	// reach, then provinces (lava fields need the volcanoes)
	g->volcanoes = volcanoes(code, *g);
	{
		int32_t cell = code.size / GRID, half = code.size / 2;
		std::vector<std::vector<uint16_t>> reach(n);
		for (size_t k = 0; k < g->volcanoes.size(); k++) {
			const Volcano &v = g->volcanoes[k];
			int32_t vcx = (v.x + half) / cell, vcz = (half - v.z) / cell, r = v.radius / cell + 2;
			for (int32_t dz = -r; dz <= r; dz++)
				for (int32_t dx = -r; dx <= r; dx++) {
					int32_t cz = vcz + dz;
					if (cz >= 0 && cz < GRID)
						reach[Grid::index(((vcx + dx) % GRID + GRID) % GRID, cz)].push_back((uint16_t)k);
				}
		}
		g->volcano_start.assign(n + 1, 0);
		for (size_t i = 0; i < n; i++) {
			g->volcano_start[i + 1] = g->volcano_start[i] + (uint32_t)reach[i].size();
			g->volcano_list.insert(g->volcano_list.end(), reach[i].begin(), reach[i].end());
		}
	}
	g->province = provinces(code, *g);
	return g;
}

} // namespace

std::shared_ptr<const Grid> gridFor(const WorldCode &code)
{
	// A few recent worlds: the map generator's threads share one, and the
	// preview reuses it while only the date changes.
	static std::mutex lock;
	static std::vector<std::pair<std::string, std::shared_ptr<const Grid>>> recent;
	{
		std::lock_guard<std::mutex> guard(lock);
		for (auto &entry : recent)
			if (entry.first == code.text)
				return entry.second;
	}
	std::shared_ptr<const Grid> built = build(code);
	std::lock_guard<std::mutex> guard(lock);
	for (auto &entry : recent)
		if (entry.first == code.text)
			return entry.second; // another thread finished first: use theirs
	recent.insert(recent.begin(), { code.text, built });
	if (recent.size() > 4)
		recent.pop_back();
	return built;
}

} // namespace terragen
