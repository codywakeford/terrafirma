// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "test.h"

#include "planet/atlas.h"
#include "terragen/code.h"
#include "terragen/world.h"
#include <chrono>
#include <cmath>
#include <cstdio>

/*
	Planets from today's largest to the Earth's size (planning/sprint-39-tiles.md):
	what the world model costs to build and to ask, how rough its land is
	block to block, and whether the atlas lays it out. Prints a SCALE line
	a radius; asserts only what must hold at every size.
*/
class TestScale : public TestBase {
public:
	TestScale() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestScale"; }

	void runTests(IGameDef *gamedef);

	void testEverySize();
};

static TestScale g_test_instance;

void TestScale::runTests(IGameDef *gamedef)
{
	TEST(testEverySize);
}

void TestScale::testEverySize()
{
	using Clock = std::chrono::steady_clock;
	const auto ms = [](Clock::time_point a) { return std::chrono::duration<double, std::milli>(Clock::now() - a).count(); };
	for (double radius : { 2607.0, 10000.0, 50000.0, 250000.0, 1000000.0, 6371000.0 }) {
		terragen::WorldCode code = terragen::WorldCode::decode("TF6.dGVycmE");
		code.size = (int32_t)std::lround(2 * M_PI * radius);

		Clock::time_point t = Clock::now();
		const terragen::World world(code);
		const double built = ms(t);

		// a walk east along 40° north, a block of longitude at a time (cos 40°
		// of a block along the ground), in 64 stretches round the world
		const terragen::fix v = terragen::ratio(5, 18);
		const auto at = [&](int32_t x) { return terragen::World::place(terragen::ratio(((x % code.size) + code.size) % code.size, code.size), v); };
		const terragen::fix sun = world.sunlight(at(0), std::nullopt);
		int32_t low = INT32_MAX, high = INT32_MIN;
		double steps = 0, steep = 0;
		size_t asked = 0;
		t = Clock::now();
		for (int s = 0; s < 64; s++) {
			const int32_t x0 = (int32_t)((int64_t)code.size * s / 64);
			int32_t was = world.ground(at(x0), sun).terrain;
			for (int32_t x = x0 + 1; x < x0 + 256; x++) {
				const int32_t h = world.ground(at(x), sun).terrain;
				low = std::min(low, h);
				high = std::max(high, h);
				steps += std::abs(h - was);
				steep = std::max(steep, (double)std::abs(h - was));
				was = h;
				asked++;
			}
		}
		const double perColumn = ms(t) * 1000 / asked;

		std::string laid;
		t = Clock::now();
		try {
			const planet::Atlas atlas = planet::Atlas::resident(terragen::Atlas::standard(radius));
			laid = "fits (" + std::to_string((int)ms(t)) + " ms)";
		} catch (const std::invalid_argument &e) {
			laid = e.what();
		}
		std::printf("SCALE radius=%.0f round=%d built=%.0fms column=%.2fus heights=%d..%d step=%.3f steepest=%.0f atlas=%s\n",
				radius, code.size, built, perColumn, low, high, steps / asked, steep, laid.c_str());
		UASSERT(high > low);
	}

	// the largest planet whose every tile is resident at once
	double fits = 1000, refused = 100000;
	while (refused - fits > 50) {
		const double r = (fits + refused) / 2;
		try {
			const planet::Atlas atlas = planet::Atlas::resident(terragen::Atlas::standard(r));
			fits = r;
		} catch (const std::invalid_argument &) {
			refused = r;
		}
	}
	const planet::Atlas largest = planet::Atlas::resident(terragen::Atlas::standard(fits));
	std::printf("SCALE resident up to radius %.0f (%zu tiles of %d)\n", fits, largest.tiles().size(),
			planet::Atlas::SLOTS * planet::Atlas::SLOTS);
}
