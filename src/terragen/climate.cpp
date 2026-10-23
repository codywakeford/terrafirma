// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "climate.h"
#include "grid.h"
#include "noise.h"
#include "sun.h"
#include <algorithm>
#include <cmath>

namespace terragen {

// ---- circulation (shared with src/fluid) -----------------------------------

/*
	Held & Hou (1980): rising equatorial air keeps its spin as it moves
	poleward, speeding eastward until it can't; the Hadley cell ends there,
	at √(5/3 · g·H·Δ / (Ω²a²)). Rhines (1975): beyond, eddies organise into
	jets about π√(2U/β) apart. Calibrated so Earth gets ~30° and one jet.
*/
static const double CONTRAST = 0.3;    // equator-to-pole contrast, as a share of the mean
static const double SURFACE = 1.0 / 8; // surface wind as a share of the wind aloft
static const double LIFT = 0.02;       // m/s
static const double PI_D = 3.14159265358979323846;
static const double COS_45 = 0.70710678118654752440;

Circulation circulate(double radius, double gravity, double day, double depth)
{
	double omega = 2 * PI_D / day;
	double edge = std::sqrt(5.0 / 3.0 * gravity * depth * CONTRAST / (omega * omega * radius * radius));
	edge = std::min(edge, PI_D / 2);
	double upper = std::min(gravity * depth * CONTRAST / (omega * radius), 0.3 * std::sqrt(gravity * depth));
	int32_t jets = 0;
	double span = radius * (PI_D / 2 - edge);
	if (span > 0.05 * radius) {
		double beta = 2 * omega * COS_45 / radius;
		double spacing = PI_D * std::sqrt(2 * upper / beta);
		jets = std::max<int32_t>(1, (int32_t)(span / spacing + 0.5));
	}
	double surface = upper * SURFACE;
	return { edge, jets, 0.7 * surface, surface, LIFT };
}

const Circulation &earthCirculation()
{
	// the weather layer: 1.6 scale heights of Earth's air (8.4 km)
	static const Circulation c = circulate(6.371e6, 9.81, 86400, 1.6 * 8400);
	return c;
}

Flow flow(const Circulation &c, fix lat, fix itcz)
{
	fix hadley = (fix)(c.hadley * ONE);
	fix north = std::min(HALF_PI, itcz + hadley), south = std::max(-HALF_PI, itcz - hadley);
	if (lat >= south && lat <= north) {
		fix side = lat >= itcz ? ONE : -ONE;
		fix edge = side > 0 ? north : south;
		fix s = edge == itcz ? 0 : div(lat - itcz, edge - itcz);
		fix sn = sin(mul(PI, s));
		return { -mul(ratio(7, 10), sn), -mul(side, mul(ratio(28, 100), sn)), cos(mul(PI, s)) };
	}
	if (c.jets == 0) // the cells reach the poles: only the polar cap, sinking
		return { 0, 0, -ONE };
	// beyond: westerly bands (rising at their poleward front), then easterly ones
	fix pole = lat > 0 ? ONE : -ONE;
	fix edge = pole > 0 ? north : -south;
	fix x = div(fabsx(lat) - edge, HALF_PI - edge) * (2 * c.jets);
	int32_t band = std::min(floorInt(x), 2 * c.jets - 1);
	fix f = x - toFix(band);
	fix s = sin(mul(PI, f));
	if (band % 2 == 0)
		return { s, mul(pole, mul(ratio(2, 10), s)), -cos(mul(PI, f)) };
	return { -s / 2, -mul(pole, s / 10), cos(mul(PI, f)) };
}

// ---- temperature ------------------------------------------------------------

fix heatAt(const WorldCode &c, fix u, fix v, fix sun, fix e)
{
	return mul(sun - ratio(55, 100), ratio(22, 10)) + c.star->warmth + mul(c.temperature, ratio(6, 10))
			+ mul(fbm(u, v, c.seed_hash + HEAT, 3, 3, HALF), ratio(2, 10)) - mul(maxx(0, e), ratio(9, 10));
}

// ---- the climate --------------------------------------------------------------

namespace {

const int SWEEPS = 40;          // passes of the moisture march (it settles in ~30)
const int AVERAGED = 8;         // the last passes, averaged
const int32_t WARMTH_BLUR = 6;  // cells: how far land and sea heat spreads (for thermal lows)
// tuned against the fluid layer on an ocean world (wet ITCZ, dry 30°, a storm track)
const fix MONSOON = toFix(12);  // wind per unit of heat difference across a cell, westerly units
const fix THERMAL = toFix(5);   // rising per unit of warmth above the latitude's mean, lift units
const fix STABLE = toFix(8);    // sinking per unit of chill from a cold current, lift units
const fix SLOPE = toFix(18);    // rising per unit of elevation climbed per cell, lift units
const fix RECYCLE = ratio(12, 10);   // land gives back up to this much of its rain (plants draw on the soil it filled)
const fix EVAPORATE = ratio(8, 100); // of the air's shortfall from saturation over water, made up per pass
const fix DRIZZLE = ratio(5, 1000);  // of the vapour, what falls per pass anyway
const fix CONVERGE = ratio(15, 100); // rising air draws in this share of its vapour per pass, per unit of lift
const fix RAINOUT = ratio(5, 10);   // of the vapour above what the air holds, what falls per pass

struct Cells
{
	int32_t n = CLIMATE;
	int32_t at(int32_t x, int32_t y) const { return std::clamp(y, 0, n - 1) * n + ((x % n) + n) % n; }
	fix lat(int32_t y) const { return mul(HALF - ratio(2 * y + 1, 2 * n), PI); }
	fix v(int32_t y) const { return ratio(2 * y + 1, 2 * n); }
	fix u(int32_t x) const { return ratio(2 * x + 1, 2 * n); }
	// bilinear at fractional cell coordinates (x wraps, y clamps)
	fix sample(const std::vector<fix> &f, fix x, fix y) const
	{
		int32_t x0 = floorInt(x), y0 = floorInt(y);
		fix tx = fraction(x), ty = fraction(y);
		return lerp(lerp(f[at(x0, y0)], f[at(x0 + 1, y0)], tx), lerp(f[at(x0, y0 + 1)], f[at(x0 + 1, y0 + 1)], tx), ty);
	}
};

// Spread a field over `r` cells each way (a box, twice: near a gaussian).
std::vector<fix> blur(const Cells &g, std::vector<fix> f, int32_t r)
{
	std::vector<fix> t(f.size());
	for (int pass = 0; pass < 2; pass++) {
		for (int32_t y = 0; y < g.n; y++)
			for (int32_t x = 0; x < g.n; x++) {
				int64_t sum = 0;
				for (int32_t k = -r; k <= r; k++)
					sum += f[g.at(x + k, y)];
				t[g.at(x, y)] = (fix)(sum / (2 * r + 1));
			}
		for (int32_t y = 0; y < g.n; y++)
			for (int32_t x = 0; x < g.n; x++) {
				int64_t sum = 0;
				for (int32_t k = -r; k <= r; k++)
					sum += t[g.at(x, y + k)];
				f[g.at(x, y)] = (fix)(sum / (2 * r + 1));
			}
	}
	return f;
}

// How much vapour the air holds at a temperature (relative): doubles every half unit of heat.
fix capacity(fix heat)
{
	return exp2(heat * 2);
}


/*
	Ocean currents, as the gyres winds drive. In each ocean the trades push
	water west and the westerlies push it east, so it circles: in the
	subtropics a narrow, fast, warm current runs poleward along the ocean's
	western edge (off east coasts: the Gulf Stream, the Kuroshio) and a
	broad, cold one runs toward the equator along its eastern edge (off west
	coasts: the Canary, Humboldt and Benguela currents, with upwelling). In
	the subpolar gyres beyond the westerlies it turns the other way round:
	warm water drifts to the eastern side (Norway's), cold runs down the
	western (Labrador's). What results is how much warmer or colder each
	stretch of sea is than its latitude, carried a little way onto the land.
*/
const int32_t COAST_REACH = 40;  // cells: how far across an ocean a coast's current is felt
const fix CURRENT_HEAT = ratio(28, 100); // heat a full-strength current carries (~10 °C)
const int32_t ONSHORE = 3;       // cells: how far a current's warmth or chill reaches inland

// 1 at `centre`, falling to 0 at ± `width` (radians)
fix bell(fix x, fix centre, fix width)
{
	fix d = div(x - centre, width);
	return maxx(0, ONE - mul(d, d));
}

std::vector<fix> currents(const Cells &g, const std::vector<fix> &elevation)
{
	size_t n = elevation.size();
	std::vector<fix> sea(n, 0);
	const fix degree = ratio(31416, 1800000); // π/180
	for (int32_t y = 0; y < g.n; y++) {
		fix lat = g.lat(y), away = fabsx(lat);
		fix subtropical = bell(away, 28 * degree, 20 * degree), subpolar = bell(away, 58 * degree, 12 * degree);
		fix tropical = bell(away, 0, 12 * degree);
		for (int32_t x = 0; x < g.n; x++) {
			int32_t i = g.at(x, y);
			if (elevation[i] >= 0)
				continue;
			// the distance to land across the ocean, each way along the row
			int32_t west = COAST_REACH, east = COAST_REACH;
			for (int32_t k = 1; k < COAST_REACH && west == COAST_REACH; k++)
				if (elevation[g.at(x - k, y)] >= 0) west = k;
			for (int32_t k = 1; k < COAST_REACH && east == COAST_REACH; k++)
				if (elevation[g.at(x + k, y)] >= 0) east = k;
			// the western boundary current is narrow, the eastern broad; none without a coast
			fix western = west < COAST_REACH ? exp2(-ratio(west, 3)) : 0;
			fix eastern = east < COAST_REACH ? exp2(-ratio(east, 8)) : 0;
			fix poleward = mul(subtropical, western - mul(eastern, ratio(7, 10)))
					+ mul(subpolar, mul(eastern, ratio(8, 10)) - mul(western, ratio(6, 10)));
			// and on the equator, cold water welling up off the eastern side
			fix upwelling = mul(tropical, mul(eastern, ratio(4, 10)));
			sea[i] = mul(poleward - upwelling, CURRENT_HEAT);
		}
	}
	// currents spread and mix (the row-by-row distances leave streaks), then
	// carry onto the coasts, thinning inland
	std::vector<fix> mixed = blur(g, sea, 2);
	for (size_t i = 0; i < n; i++)
		if (elevation[i] < 0)
			sea[i] = mixed[i];
	std::vector<fix> spread = blur(g, sea, ONSHORE);
	for (size_t i = 0; i < n; i++)
		if (elevation[i] >= 0)
			sea[i] = mul(spread[i], ratio(7, 10));
	return sea;
}

// One season's rain, from its sun (date), the ground and the currents off it.
std::vector<fix> season(const WorldCode &code, const Cells &g, const std::vector<fix> &elevation,
		const std::vector<fix> &sea, fix date)
{
	size_t n = elevation.size();
	const Circulation &circ = earthCirculation();
	fix itcz = mul(declination(code.tilt, date) * 45, ratio(1, 100)); // the rising branch follows the sun about halfway (thermal lows carry it further over land)

	std::vector<fix> heat(n);
	for (int32_t y = 0; y < g.n; y++) {
		fix sun = sunlight(code, g.v(y), date);
		for (int32_t x = 0; x < g.n; x++)
			heat[g.at(x, y)] = heatAt(code, g.u(x), g.v(y), sun, maxx(0, elevation[g.at(x, y)])) + sea[g.at(x, y)];
	}
	// the air's temperature over hundreds of km; and the same brought down to
	// sea level (potential temperature), which decides thermal lows and highs:
	// land warmer than its latitude draws air in and lifts it, and a high
	// plateau is warm air, not cold (a summer heat low, like Tibet's)
	std::vector<fix> air = blur(g, heat, WARMTH_BLUR), potential(n);
	for (size_t i = 0; i < n; i++)
		potential[i] = heat[i] + mul(maxx(0, elevation[i]), ratio(9, 10));
	std::vector<fix> warmth = blur(g, potential, WARMTH_BLUR);
	std::vector<fix> zonal(g.n);
	for (int32_t y = 0; y < g.n; y++) {
		int64_t sum = 0;
		for (int32_t x = 0; x < g.n; x++)
			sum += warmth[g.at(x, y)];
		zonal[y] = (fix)(sum / g.n);
	}

	std::vector<fix> wu(n), wv(n), lift(n);
	for (int32_t y = 0; y < g.n; y++) {
		Flow f = flow(circ, g.lat(y), itcz);
		for (int32_t x = 0; x < g.n; x++) {
			int32_t i = g.at(x, y);
			// the mean wind, bent toward warmer air (sea breezes at the scale of continents)
			fix u = f.u + mul(warmth[g.at(x + 1, y)] - warmth[g.at(x - 1, y)], MONSOON / 2);
			fix v = f.v + mul(warmth[g.at(x, y - 1)] - warmth[g.at(x, y + 1)], MONSOON / 2);
			// wind blowing up the land is lifted by it
			auto land = [&](int32_t cx, int32_t cy) { return maxx(0, elevation[g.at(cx, cy)]); };
			fix climb = mul(u, land(x + 1, y) - land(x - 1, y)) / 2 + mul(v, land(x, y - 1) - land(x, y + 1)) / 2;
			wu[i] = u;
			wv[i] = v;
			// air over a cold current is chilled from below and won't rise (the coastal deserts)
			lift[i] = f.w + mul(warmth[i] - zonal[y], THERMAL) + mul(climb, SLOPE) + mul(std::min<fix>(sea[i], 0), STABLE);
		}
	}

	// what the air can hold: at the surface (what evaporation fills it to),
	// and once lifted (rising air holds less, sinking air more)
	std::vector<fix> surface(n), holds(n);
	for (size_t i = 0; i < n; i++) {
		// the air's temperature is set over hundreds of km, not by the patch
		// under it (hot desert ground doesn't make its air hold more); a
		// quarter of the local heat keeps mountains cooling what crosses them
		surface[i] = capacity((3 * air[i] + heat[i]) / 4);
		holds[i] = mul(surface[i], exp2(-mul(lift[i], ratio(12, 10))));
	}

	// the moisture march: vapour carried along the wind, made up toward
	// saturation over water (and from wet land), rained out where the air,
	// lifted, holds less than it carries
	std::vector<fix> q(n, 0), rain(n, 0), total(n, 0), next(n);
	for (int pass = 0; pass < SWEEPS; pass++) {
		for (int32_t y = 0; y < g.n; y++)
			for (int32_t x = 0; x < g.n; x++) {
				int32_t i = g.at(x, y);
				// where this air was a pass ago (a cell per pass at unit wind; rows run south)
				fix from = g.sample(q, toFix(x) - wu[i], clamp(toFix(y) + wv[i], 0, toFix(g.n - 1)));
				fix shortfall = mul(maxx(surface[i] - from, 0), EVAPORATE);
				fix fed = elevation[i] < 0 ? shortfall : std::min(shortfall, mul(rain[i], RECYCLE));
				// rising air is fed by low air converging from all around (the
				// one mean wind can't carry that), never past saturation at the surface
				fix drawn = lift[i] > 0 ? std::min(mul(from, mul(lift[i], CONVERGE)), maxx(surface[i] - from - fed, 0)) : 0;
				fix carried = from + fed + drawn;
				fix fall = mul(maxx(carried - holds[i], 0), RAINOUT) + mul(carried, DRIZZLE);
				next[i] = carried - fall;
				rain[i] = fall;
			}
		// a little mixing north and south: the eddies the mean wind leaves out
		for (int32_t y = 0; y < g.n; y++)
			for (int32_t x = 0; x < g.n; x++)
				q[g.at(x, y)] = mul(next[g.at(x, y)], ratio(8, 10)) + mul(next[g.at(x, y - 1)] + next[g.at(x, y + 1)], ratio(1, 10));
		if (pass >= SWEEPS - AVERAGED)
			for (size_t i = 0; i < n; i++)
				total[i] += rain[i] / AVERAGED;
	}
	return total;
}

} // namespace

Climate climate(const WorldCode &code, const std::vector<fix> &elevation)
{
	Cells g;
	if ((int32_t)elevation.size() != g.n * g.n)
		throw std::invalid_argument("climate: the elevation must be CLIMATE² cells");
	Climate c;
	c.sea = currents(g, elevation);
	c.rain[0] = season(code, g, elevation, c.sea, ratio(1, 4));
	c.rain[1] = season(code, g, elevation, c.sea, ratio(3, 4));
	// relative to the median year's rain over land (half the land is wetter
	// than 1; every cell on a world of sea), then the Humidity setting (×½ .. ×2)
	std::vector<fix> years;
	for (bool all : { false, true }) {
		for (size_t i = 0; i < elevation.size(); i++)
			if (all || elevation[i] >= 0)
				years.push_back((c.rain[0][i] + c.rain[1][i]) / 2);
		if (!years.empty())
			break;
	}
	std::nth_element(years.begin(), years.begin() + years.size() / 2, years.end());
	fix median = std::max<fix>(years[years.size() / 2], 1);
	fix wetter = exp2(code.humidity);
	for (int k = 0; k < 2; k++)
		for (fix &r : c.rain[k])
			r = maxx(mul(div(r, median), wetter), ratio(2, 100));
	return c;
}

fix sunlight(const WorldCode &code, fix v, std::optional<fix> date)
{
	fix latitude = mul(HALF - v, PI);
	fix yearly = annualInsolation(latitude, code.tilt);
	if (!date)
		return yearly;
	// that day's sun, plus a share of the year's (land and sea store heat)
	fix today = insolation(latitude, declination(code.tilt, *date));
	return mul(today, ratio(6, 10)) + mul(yearly, ratio(4, 10));
}

} // namespace terragen
