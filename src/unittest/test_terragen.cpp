// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "test.h"

#include "terragen/body.h"
#include "terragen/calendar.h"
#include "terragen/code.h"
#include "terragen/fixed.h"
#include "terragen/geology.h"
#include "terragen/noise.h"
#include "terragen/preview.h"
#include "terragen/sun.h"
#include "terragen/world.h"
#include <algorithm>
#include <string>

using namespace terragen;

class TestTerragen : public TestBase {
public:
	TestTerragen() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestTerragen"; }

	void runTests(IGameDef *gamedef);

	void testDecodeDefaults();
	void testDecodeParams();
	void testDecodeRejects();
	void testTrig();
	void testNoiseWrapsEastWest();
	void testSun();
	void testSunPath();
	void testCalendar();
	void testTemperature();
	void testLogExp();
	void testBodies();
	void testCoordinates();
	void testLandShareExact();
	void testWaterDrainsToSea();
	void testWaterFlowsDownhill();
	void testRiversOnLand();
	void testLakesAreFlat();
	void testPlates();
	void testVolcanoes();
	void testStrata();
	void testOresAndCaves();
	void testCover();
	void testDeterminismGolden();
};

static TestTerragen g_test_instance;

void TestTerragen::runTests(IGameDef *gamedef)
{
	TEST(testDecodeDefaults);
	TEST(testDecodeParams);
	TEST(testDecodeRejects);
	TEST(testTrig);
	TEST(testNoiseWrapsEastWest);
	TEST(testSun);
	TEST(testSunPath);
	TEST(testCalendar);
	TEST(testTemperature);
	TEST(testLogExp);
	TEST(testBodies);
	TEST(testCoordinates);
	TEST(testLandShareExact);
	TEST(testWaterDrainsToSea);
	TEST(testWaterFlowsDownhill);
	TEST(testRiversOnLand);
	TEST(testLakesAreFlat);
	TEST(testPlates);
	TEST(testVolcanoes);
	TEST(testStrata);
	TEST(testOresAndCaves);
	TEST(testCover);
	TEST(testDeterminismGolden);
}

// Codes for the water tests: default, a supercontinent, an archipelago (as the page encodes them).
static const char *const WATER_CODES[] = {
	"TF3.dGVycmE",
	"TF3.dGVycmE.AgIAplmZgICAgICAglU",
	"TF3.dGVycmE.AgL_QNlNgKaAgICAglU",
};

static bool near(fix a, fix b, fix tolerance)
{
	return fabsx(a - b) <= tolerance;
}

// Codes produced by the page's encodeWorldCode (ui/src/worldgen/config.ts).
void TestTerragen::testDecodeDefaults()
{
	WorldCode w = WorldCode::decode("TF3.YWJj");
	UASSERTEQ(std::string, w.seed, "abc");
	UASSERTEQ(int, w.size, 8192);
	UASSERTEQ(std::string, w.star->id, "yellow");
	UASSERTEQ(int, w.continents, 4);
	UASSERTEQ(int, roundInt(w.tilt), 23);
	UASSERTEQ(int, w.year, 48);
	UASSERT(near(w.land, ratio(4, 10), 256));
	UASSERT(near(w.temperature, 0, 600));
}

void TestTerragen::testDecodeParams()
{
	// size 4096, red dwarf, continents 6, tilt 40, everything else default
	WorldCode w = WorldCode::decode("TF3.YWJj.AQC2ZoCAgICAgICA41U");
	UASSERTEQ(int, w.size, 4096);
	UASSERTEQ(std::string, w.star->id, "red");
	UASSERTEQ(int, w.continents, 6);
	UASSERTEQ(int, roundInt(w.tilt), 40);
	UASSERTEQ(int, w.year, 48);
}

void TestTerragen::testDecodeRejects()
{
	EXCEPTION_CHECK(CodeError, WorldCode::decode("XX.YWJj"));
	EXCEPTION_CHECK(CodeError, WorldCode::decode("TF1.YWJj")); // an older generator: refused, not reinterpreted
	EXCEPTION_CHECK(CodeError, WorldCode::decode("TF2.YWJj")); // before plates and geology: refused too
	EXCEPTION_CHECK(CodeError, WorldCode::decode("TF3."));
	EXCEPTION_CHECK(CodeError, WorldCode::decode("TF3.YWJj.AAA"));
	EXCEPTION_CHECK(CodeError, WorldCode::decode("TF3.YW*j"));
}

void TestTerragen::testTrig()
{
	const fix tol = ONE / 500;
	UASSERT(near(terragen::sin(HALF_PI), ONE, tol));
	UASSERT(near(terragen::cos(0), ONE, tol));
	UASSERT(near(terragen::sin(PI), 0, tol));
	UASSERT(near(terragen::cos(PI), -ONE, tol));
	UASSERT(near(terragen::sin(-HALF_PI), -ONE, tol));
	UASSERT(near(terragen::sin(TWO_PI + HALF_PI), ONE, tol)); // reduction
	UASSERT(near(terragen::acos(0), HALF_PI, tol));
	UASSERT(near(terragen::acos(ONE), 0, tol));
	UASSERT(near(terragen::acos(-ONE), PI, tol));
	UASSERT(near(terragen::atan2(ONE, -ONE), PI - PI / 4, tol));
	UASSERT(near(terragen::atan2(-ONE, -ONE), -(PI - PI / 4), tol));
	UASSERT(near(terragen::sqrt(toFix(4)), toFix(2), 2));
}

void TestTerragen::testNoiseWrapsEastWest()
{
	// the east edge is the west edge, exactly
	for (fix v = 0; v < ONE; v += ONE / 7) {
		UASSERTEQ(fix, fbm(0, v, 1234, 5, 6, HALF), fbm(ONE, v, 1234, 5, 6, HALF));
		UASSERTEQ(fix, value(0, v, 99, 3), value(ONE, v, 99, 3));
	}
}

void TestTerragen::testSun()
{
	const fix tol = ONE / 50;
	UASSERT(near(insolation(0, 0), ONE, tol));                       // equator at equinox
	fix lat60 = mul(PI, ratio(1, 3));
	UASSERT(annualInsolation(0, toFix(23)) > annualInsolation(lat60, toFix(23)));
	fix lat80 = mul(PI, ratio(80, 180));
	fix winter = declination(toFix(23), ratio(3, 4));               // northern winter
	UASSERT(near(insolation(lat80, winter), 0, tol));                // polar night
	fix summer = declination(toFix(23), ratio(1, 4));
	UASSERT(insolation(lat80, summer) > ONE);                        // midnight sun
}

static fix degrees(int32_t d)
{
	return (fix)(((int64_t)d * PI) / 180);
}

void TestTerragen::testSunPath()
{
	const fix tol = degrees(1);
	fix equinox = 0, summer = declination(toFix(23), ratio(1, 4)), winter = -summer;
	UASSERT(near(elevation(0, equinox, HALF), HALF_PI, tol));            // overhead at the equator
	UASSERT(near(elevation(degrees(60), equinox, HALF), degrees(30), tol));
	UASSERT(near(elevation(degrees(60), summer, HALF), degrees(53), tol)); // 90 − 60 + 23
	UASSERT(near(elevation(0, equinox, ratio(1, 4)), 0, tol));           // sunrise at six
	UASSERT(elevation(degrees(45), equinox, 0) < 0);                      // midnight
	UASSERT(elevation(degrees(80), winter, HALF) < 0);                    // polar night: no sunrise
	UASSERT(elevation(degrees(80), summer, 0) > 0);                       // midnight sun
	UASSERT(near(elevation(-degrees(60), winter, HALF), degrees(53), tol)); // the south mirrors

	const fix share = ONE / 100;
	UASSERT(near(dayShare(degrees(50), equinox), HALF, share));
	UASSERT(dayShare(degrees(50), summer) > dayShare(degrees(50), equinox));
	UASSERT(dayShare(-degrees(50), summer) < HALF);
	UASSERTEQ(fix, dayShare(degrees(80), winter), 0);
	UASSERTEQ(fix, dayShare(degrees(80), summer), ONE);
}

void TestTerragen::testCalendar()
{
	Calendar c = calendar(28, 30, ratio(1, 4));
	UASSERTEQ(uint32_t, c.year, 1u);
	UASSERTEQ(uint32_t, c.day, 2u);
	UASSERTEQ(fix, c.date, (fix)((2 * (int64_t)ONE + ratio(1, 4)) / 28));
	UASSERT(calendar(28, 27, ONE - 1).date < ONE);                       // never reaches the next year

	UASSERT(season(0, degrees(45)) == Season::Spring);
	UASSERT(season(ratio(3, 10), degrees(45)) == Season::Summer);
	UASSERT(season(ratio(3, 10), -degrees(45)) == Season::Winter);
	UASSERT(season(ratio(9, 10), -degrees(45)) == Season::Summer);
	UASSERT(season(ratio(6, 10), 0) == Season::Autumn);
}

void TestTerragen::testTemperature()
{
	World w(WorldCode::decode(WATER_CODES[0]));
	int32_t quarter = w.code().size / 4;
	int32_t north = quarter + quarter / 2;   // ≈ 67°N
	fix midsummer = ratio(1, 4), midwinter = ratio(3, 4);
	fix afternoon = ratio(5, 8), night = ratio(1, 8);
	for (int32_t x = -quarter; x <= quarter; x += quarter / 2) {
		UASSERT(w.temperature(x, quarter, midsummer, afternoon) > w.temperature(x, quarter, midwinter, afternoon));
		UASSERT(w.temperature(x, -quarter, midwinter, afternoon) > w.temperature(x, -quarter, midsummer, afternoon));
		UASSERT(w.temperature(x, 0, ratio(1, 8), afternoon) > w.temperature(x, 0, ratio(1, 8), night));
		UASSERT(w.temperature(x, 0, 0, HALF) > w.temperature(x, north, 0, HALF));
	}
	fix t = w.temperature(0, 0, 0, HALF);
	UASSERT(t > toFix(-60) && t < toFix(60));
}

void TestTerragen::testLogExp()
{
	UASSERTEQ(fix, log2(ONE), 0);
	UASSERTEQ(fix, log2(toFix(8)), toFix(3));
	UASSERTEQ(fix, exp2(toFix(3)), toFix(8));
	UASSERT(near(log2(ratio(3, 2)), 38336, 4));          // 0.58496
	UASSERT(near(exp2(HALF), 92682, 4));                  // √2
	UASSERT(near(exp(ONE), 178145, 16));                  // e
	UASSERT(near(pow(toFix(9), HALF), toFix(3), 8));
	for (fix v = ratio(1, 64); v < toFix(1000); v += v / 3 + 1) // round trips across the range
		UASSERT(near(exp2(log2(v)), v, std::max<fix>(4, v / 2000)));
	UASSERTEQ(fix, exp2(toFix(-40)), 0);
	UASSERTEQ(fix, exp2(toFix(40)), INT32_MAX);
}

static fix real(double v)
{
	return (fix)(v * ONE);
}

// Our solar system from causes alone: every body must come out as itself.
void TestTerragen::testBodies()
{
	StarCauses sun{ 2, real(4.5), real(0.5) };
	std::vector<BodyCauses> c = {
		{ -1, real(1), real(0.017), real(1), real(0.35), real(0.6), real(24), true },          // Earth
		{ -1, real(0.72), real(0.007), real(0.82), real(0.3), real(0.6), real(5832), false },   // Venus
		{ -1, real(1.52), real(0.09), real(0.107), real(0.45), real(0.5), real(24.6), false },  // Mars
		{ -1, real(0.39), real(0.2), real(0.055), real(0.1), real(0.05), real(1408), false },   // Mercury
		{ 0, real(60), real(0.055), real(0.0123), real(0.35), real(0.05), real(655), false },   // the Moon
		{ -1, real(5.2), real(0.05), real(318), real(0.8), real(0.6), real(10), false },        // Jupiter
		{ 5, real(5.9), real(0.0041), real(0.015), real(0.55), real(0.05), real(42), false },   // Io
		{ 5, real(9.4), real(0.009), real(0.008), real(0.8), real(0.6), real(85), false },      // Europa
		{ -1, real(9.5), real(0.05), real(95), real(0.85), real(0.6), real(10.7), false },      // Saturn
		{ 8, real(20), real(0.029), real(0.0225), real(0.85), real(0.7), real(383), false },    // Titan
	};
	std::vector<BodyCard> b = bodies(sun, c);
	const Kind kinds[] = { Kind::Temperate, Kind::Hothouse, Kind::ColdDesert, Kind::Airless, Kind::Airless,
		Kind::Giant, Kind::Volcanic, Kind::IceShell, Kind::Giant, Kind::Hydrocarbon };
	for (size_t i = 0; i < b.size(); i++)
		UASSERT(b[i].kind == kinds[i]);

	const BodyCard &earth = b[0];
	UASSERT(earth.pressure > real(0.7) && earth.pressure < real(1.6));
	UASSERT(earth.temperature > toFix(278) && earth.temperature < toFix(300));
	UASSERT(earth.gases[(int)Gas::N2] > real(0.7) && earth.gases[(int)Gas::O2] > real(0.1));
	UASSERT(earth.field > HALF && earth.ocean && earth.plates);
	UASSERT(b[1].pressure > toFix(5));                      // Venus: thick CO₂
	UASSERT(b[1].gases[(int)Gas::CO2] > real(0.9));
	UASSERT(b[4].locked && b[6].tidal > toFix(10));        // the Moon faces us; Io is tidally heated

	// insides: Earth's molten core is about half its radius; Europa hides an
	// ocean under its ice; Jupiter is mostly envelope
	auto layer = [](const BodyCard &card, const char *name) -> const Layer * {
		for (const Layer &l : card.layers)
			if (std::string(l.name) == name)
				return &l;
		return nullptr;
	};
	UASSERT(layer(earth, "core") && layer(earth, "core")->molten);
	UASSERT(layer(earth, "core")->outer > real(0.45) && layer(earth, "core")->outer < real(0.65));
	UASSERT(layer(earth, "ocean") && !layer(b[2], "ocean"));
	UASSERT(layer(b[7], "ocean") && layer(b[7], "ice") && layer(b[7], "ocean")->outer < layer(b[7], "ice")->outer);
	UASSERT(layer(b[5], "envelope") && !layer(b[5], "crust"));

	auto told = [](const BodyCard &card, Event e) {
		for (const Moment &m : card.history)
			if (m.event == e)
				return true;
		return false;
	};
	UASSERT(told(b[2], Event::FieldFades) && told(b[2], Event::OceansFreeze)); // Mars's story
	UASSERT(told(b[1], Event::Runaway) && !told(b[3], Event::Runaway));
	UASSERT(told(earth, Event::LifeAppears) && told(earth, Event::OxygenRises));

	// the same causes, the same cards
	std::vector<BodyCard> again = bodies(sun, c);
	for (size_t i = 0; i < b.size(); i++)
		UASSERT(again[i].temperature == b[i].temperature && again[i].pressure == b[i].pressure);

	// impossible systems are refused
	std::vector<BodyCauses> bad = { c[4] }; // a moon with no planet before it
	EXCEPTION_CHECK(std::invalid_argument, bodies(sun, bad));
}

void TestTerragen::testCoordinates()
{
	World w(WorldCode::decode("TF3.YWJj"));
	int32_t half = w.code().size / 2;
	UASSERTEQ(fix, w.u(-half), 0);
	UASSERTEQ(fix, w.u(half), 0);       // wraps
	UASSERTEQ(fix, w.v(half), 0);       // north edge
	UASSERTEQ(fix, w.v(-half), ONE);    // south edge
}

void TestTerragen::testLandShareExact()
{
	for (const char *text : WATER_CODES) {
		WorldCode code = WorldCode::decode(text);
		World w(code);
		const Grid &g = w.grid();
		int64_t land = 0, open = 0;
		for (int32_t i = 0; i < GRID * GRID; i++) {
			if (g.polar[i])
				continue;
			open++;
			land += g.base[i] >= 0;
		}
		// within a fraction of a percent of the Land setting
		fix share = ratio(land, open);
		UASSERT(near(share, code.land, ONE / 200));
	}
}

void TestTerragen::testWaterDrainsToSea()
{
	for (const char *text : WATER_CODES) {
		World w(WorldCode::decode(text));
		const Grid &g = w.grid();
		for (int32_t i = 0; i < GRID * GRID; i++) {
			if (g.polar[i] || g.base[i] < 0)
				continue;
			int32_t c = i, steps = 0;
			while (g.receiver[c] >= 0 && g.base[c] >= 0 && steps < GRID * GRID) {
				c = g.receiver[c];
				steps++;
			}
			UASSERT(g.base[c] < 0); // every land cell's water reaches the sea
		}
	}
}

void TestTerragen::testWaterFlowsDownhill()
{
	for (const char *text : WATER_CODES) {
		World w(WorldCode::decode(text));
		const Grid &g = w.grid();
		for (int32_t i = 0; i < GRID * GRID; i++) {
			int32_t r = g.receiver[i];
			if (r >= 0)
				UASSERT(g.filled[r] < g.filled[i]);
		}
	}
}

// A lake has one surface: touching lake cells share a level, so real
// (flowing) water has nothing to spill down across it.
void TestTerragen::testLakesAreFlat()
{
	for (const char *text : WATER_CODES) {
		World w(WorldCode::decode(text));
		const Grid &g = w.grid();
		int lakes = 0;
		for (int32_t cz = 0; cz < GRID; cz++) {
			for (int32_t cx = 0; cx < GRID; cx++) {
				int32_t i = Grid::index(cx, cz);
				if (!g.lake[i])
					continue;
				lakes++;
				for (int32_t dz = 0; dz <= 1; dz++) {
					for (int32_t dx = -1; dx <= 1; dx++) {
						int32_t nz = cz + dz, nx = (cx + dx + GRID) % GRID;
						if ((dz == 0 && dx <= 0) || nz >= GRID)
							continue;
						int32_t j = Grid::index(nx, nz);
						if (g.lake[j])
							UASSERTEQ(fix, g.lake_level[i], g.lake_level[j]);
					}
				}
			}
		}
		UASSERT(lakes > 0);
	}
}

// Plates cover the world; boundaries of every kind exist; mountains stand
// along the collisions.
void TestTerragen::testPlates()
{
	for (const char *text : WATER_CODES) {
		World w(WorldCode::decode(text));
		const Grid &g = w.grid();
		const Tectonics &t = g.tectonics;
		UASSERT(t.plates.size() >= 7);
		int kinds[4] = {};
		double near = 0, far = 0;
		int nn = 0, nf = 0;
		for (int32_t i = 0; i < GRID * GRID; i++) {
			UASSERT(t.plate[i] < t.plates.size());
			if (t.distance[i] == 0)
				kinds[(int)t.boundary[i]]++;
			if (g.polar[i] || g.eroded[i] < 0)
				continue;
			bool collision = t.boundary[i] == Boundary::Convergent && t.distance[i] < 5;
			(collision ? near : far) += g.eroded[i];
			(collision ? nn : nf)++;
		}
		UASSERT(kinds[(int)Boundary::Convergent] > 0 && kinds[(int)Boundary::Divergent] > 0 && kinds[(int)Boundary::Transform] > 0);
		if (nn > 50)
			UASSERT(near / nn > far / nf); // ranges along the collisions
	}
}

// Volcanoes stand where plates allow them, and some are active; a column
// at an active vent holds lava.
void TestTerragen::testVolcanoes()
{
	World w(WorldCode::decode(WATER_CODES[0]));
	const Grid &g = w.grid();
	UASSERT(!g.volcanoes.empty());
	int active = 0;
	for (const Volcano &v : g.volcanoes) {
		active += v.active;
		UASSERT(v.radius > 0 && v.height > 0 && v.crater < v.radius);
	}
	UASSERT(active > 0);
	bool lava = false;
	for (const Volcano &v : g.volcanoes) {
		if (!v.active)
			continue;
		Ground at = w.ground(v.x, v.z, w.sunlight(w.v(v.z), std::nullopt));
		Ground beside = w.ground(v.x + v.radius + 20, v.z, w.sunlight(w.v(v.z), std::nullopt));
		lava |= at.kind == Water::Lava;
		UASSERT(at.terrain > beside.terrain || at.kind == Water::Lava); // the cone stands up
	}
	UASSERT(lava);
}

// Rock follows the province: sediment over basement in basins, basalt under
// the sea floor, granite or gneiss in the shields.
void TestTerragen::testStrata()
{
	World w(WorldCode::decode(WATER_CODES[0]));
	int32_t size = w.code().size;
	int seen[(int)Province::Count] = {};
	for (int32_t k = 0; k < 4000; k++) {
		int32_t x = (k * 7919) % size - size / 2, z = (k * 104729) % (size * 8 / 10) - size * 4 / 10;
		Ground g = w.ground(x, z, w.sunlight(w.v(z), std::nullopt));
		if (g.column.polar)
			continue;
		Strata s = w.strata(x, z, g);
		seen[(int)s.province]++;
		Rock deep = w.rock(s, 400, x, g.terrain - 400, z);
		switch (s.province) {
		case Province::OceanFloor: UASSERT(deep == Rock::Basalt); break;
		case Province::Shield: UASSERT(deep == Rock::Granite || deep == Rock::Gneiss); break;
		case Province::Basin: {
			UASSERT(s.sediment >= 20);
			Rock top = w.rock(s, 0, x, g.terrain, z);
			UASSERT(top == Rock::Sandstone || top == Rock::Shale || top == Rock::Limestone || top == Rock::Obsidian);
			break;
		}
		default: break;
		}
	}
	for (int p = 0; p < (int)Province::Count; p++)
		UASSERT(seen[p] > 0); // every kind of ground somewhere
}

// Ores where their processes put them, and caves most of all in limestone.
void TestTerragen::testOresAndCaves()
{
	World w(WorldCode::decode(WATER_CODES[0]));
	int32_t size = w.code().size;
	int ores[(int)Ore::Count] = {};
	int carved[(int)Rock::Count] = {}, blocks[(int)Rock::Count] = {};
	for (int32_t k = 0; k < 3000; k++) {
		int32_t x = (k * 7919) % size - size / 2, z = (k * 104729) % (size * 8 / 10) - size * 4 / 10;
		Ground g = w.ground(x, z, w.sunlight(w.v(z), std::nullopt));
		if (g.column.polar)
			continue;
		Strata s = w.strata(x, z, g);
		for (int32_t depth = 0; depth < 140; depth++) {
			int32_t y = g.terrain - depth;
			Rock r = w.rock(s, depth, x, y, z);
			ores[(int)w.ore(s, r, depth, x, y, z)]++;
			blocks[(int)r]++;
			carved[(int)r] += w.cave(s, r, depth, x, y, z, g.kind != Water::None) != Cave::None;
		}
	}
	for (Ore o : { Ore::Copper, Ore::Lead, Ore::Iron, Ore::Magnetite, Ore::Coal })
		UASSERT(ores[(int)o] > 0);
	UASSERT(ores[(int)Ore::None] > 50 * (ores[(int)Ore::Copper] + ores[(int)Ore::Iron])); // ore is the exception
	auto share = [&](Rock r) { return blocks[(int)r] ? (double)carved[(int)r] / blocks[(int)r] : 0.0; };
	UASSERT(blocks[(int)Rock::Limestone] > 0 && share(Rock::Limestone) > 2 * share(Rock::Granite));
}

// What can grow follows warmth and water: warm wet ground grows more than
// cold or dry; beyond the treeline nothing; trees stand only where it can grow.
void TestTerragen::testCover()
{
	World w(WorldCode::decode(WATER_CODES[0]));
	int32_t size = w.code().size;
	double warm_wet = 0, cold = 0, dry = 0;
	int n_ww = 0, n_cold = 0, n_dry = 0, trees = 0, columns = 0;
	for (int32_t k = 0; k < 6000; k++) {
		int32_t x = (k * 7919) % size - size / 2, z = (k * 104729) % (size * 8 / 10) - size * 4 / 10;
		Ground g = w.ground(x, z, w.sunlight(w.v(z), std::nullopt));
		Cover c = w.cover(x, z, g);
		UASSERT(c.density >= 0 && c.density <= ONE);
		if (g.kind != Water::None || g.column.polar)
			continue;
		columns++;
		trees += c.tree;
		if (c.tree)
			UASSERT(c.density > 0);
		if (g.column.heat < -ratio(45, 100))
			UASSERT(c.density == 0); // beyond the treeline
		double d = c.density / 65536.0;
		if (g.column.heat > ratio(10, 100) && g.column.wet > ratio(20, 100)) { warm_wet += d; n_ww++; }
		else if (g.column.heat < -ratio(30, 100)) { cold += d; n_cold++; }
		else if (g.column.wet < -ratio(30, 100) && !c.bank) { dry += d; n_dry++; }
	}
	UASSERT(trees > 0 && trees < columns / 10);
	if (n_ww && n_cold) UASSERT(warm_wet / n_ww > cold / n_cold);
	if (n_ww && n_dry) UASSERT(warm_wet / n_ww > dry / n_dry);
}

void TestTerragen::testRiversOnLand()
{
	for (const char *text : WATER_CODES) {
		World w(WorldCode::decode(text));
		const Grid &g = w.grid();
		int rivers = 0;
		for (int32_t i = 0; i < GRID * GRID; i++) {
			if (!g.river(i))
				continue;
			rivers++;
			UASSERT(!g.polar[i] && g.base[i] >= 0);
		}
		UASSERT(rivers > 0);
	}
}

static void describe(const char *code)
{
	World w(WorldCode::decode(code));
	Preview p = preview(w, std::nullopt, 64);
	int counts[(int)Biome::Count] = {};
	for (uint8_t b : p.biome)
		counts[b]++;
	infostream << "terragen " << code << ":";
	for (int b = 0; b < (int)Biome::Count; b++)
		infostream << " " << BIOME_NAMES[b] << "=" << counts[b];
	infostream << std::endl;
}

// Any change to generation changes this. That is the point: a changed world
// for an existing code needs a new code version, not a silent update.
void TestTerragen::testDeterminismGolden()
{
	describe("TF3.YWJj");
	describe("TF3.dGVycmE");
	World w(WorldCode::decode("TF3.YWJj.AQC2ZoCAgICAgICA41U"));
	Preview p = preview(w, std::nullopt, 48);
	uint32_t h = 2166136261u;
	auto mix = [&](uint32_t v) { h = (h ^ v) * 16777619u; };
	for (size_t i = 0; i < p.biome.size(); i++) {
		mix((uint16_t)p.elevation[i]);
		mix((uint8_t)p.heat[i]);
		mix((uint8_t)p.wet[i]);
		mix(p.biome[i]);
	}
	// and what's underground: province, rock and ore down a few columns
	for (int32_t k = 0; k < 24; k++) {
		int32_t x = (k * 2311) % w.code().size - w.code().size / 2, z = (k * 977) % (w.code().size / 2) - w.code().size / 4;
		Ground g = w.ground(x, z, w.sunlight(w.v(z), std::nullopt));
		Strata s = w.strata(x, z, g);
		mix((uint32_t)s.province);
		for (int32_t depth = 0; depth < 120; depth += 7) {
			Rock r = w.rock(s, depth, x, g.terrain - depth, z);
			mix((uint32_t)r);
			mix((uint32_t)w.ore(s, r, depth, x, g.terrain - depth, z));
			mix((uint32_t)w.cave(s, r, depth, x, g.terrain - depth, z, g.kind != Water::None));
		}
	}
	// what the world is made of, for a human reading --info output when tuning
	int counts[(int)Biome::Count] = {};
	int16_t lo = 32767, hi = -32767;
	for (size_t i = 0; i < p.biome.size(); i++) {
		counts[p.biome[i]]++;
		lo = std::min(lo, p.elevation[i]);
		hi = std::max(hi, p.elevation[i]);
	}
	infostream << "terragen golden: " << h << ", elevation " << lo / 10000.f << ".." << hi / 10000.f << ", biomes:";
	for (int b = 0; b < (int)Biome::Count; b++)
		infostream << " " << BIOME_NAMES[b] << "=" << counts[b];
	infostream << std::endl;
	UASSERTEQ(uint32_t, h, 1213676375u); // TF3: plates, volcanoes, strata, ores, caves
}
