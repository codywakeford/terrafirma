// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "test.h"

#include "terragen/sphere.h"
#include <cmath>
#include <set>
#include <tuple>

using namespace terragen;

class TestSphere : public TestBase {
public:
	TestSphere() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestSphere"; }

	void runTests(IGameDef *gamedef);

	void testRoundTrip();
	void testEdgesLineUp();
	void testEightCorners();
	void testCellSides();
	void testFrame();
	void testLensIsTheGridAtTheEye();
	void testLensCurves();
	void testLensInFloats();
};

static TestSphere g_test_instance;

void TestSphere::runTests(IGameDef *gamedef)
{
	TEST(testRoundTrip);
	TEST(testEdgesLineUp);
	TEST(testEightCorners);
	TEST(testCellSides);
	TEST(testFrame);
	TEST(testLensIsTheGridAtTheEye);
	TEST(testLensCurves);
	TEST(testLensInFloats);
}

// The spike's planet: ~38 km round, a face 59,690 blocks across.
static const Sphere PLANET(29845);

static bool near(double a, double b, double within)
{
	return std::abs(a - b) <= within;
}

void TestSphere::testRoundTrip()
{
	const int32_t h = PLANET.half();
	for (int face = 0; face < 6; face++)
	for (double x : { -h + 0.5, -1234.25, 0.0, 777.5, h - 0.5 })
	for (double z : { -h + 0.5, -40.0, 0.0, 2001.75, h - 0.5 }) {
		const Sphere::Place p = PLANET.place(PLANET.direction({ face, x, z }));
		UASSERTEQ(int, p.face, face);
		UASSERT(near(p.x, x, 1e-6) && near(p.z, z, 1e-6));
	}
}

// Two faces meet block for block: the middle of a block's outer side is the
// middle of a block's outer side on the neighbour face, and its corners
// are corners there. (A face's own grid carried on past its edge is not
// the neighbour's: that's what settle() is for.)
void TestSphere::testEdgesLineUp()
{
	const int32_t h = PLANET.half();
	const double edge = h + 1e-7; // a hair past it, so the neighbour is the nearer face
	for (int face = 0; face < 6; face++)
	for (double along = -h; along < h; along += 498.5) { // block corners and middles
		for (const Sphere::Place &past : std::initializer_list<Sphere::Place>{
				{ face, edge, along }, { face, -edge, along }, { face, along, edge }, { face, along, -edge } }) {
			const Sphere::Place there = PLANET.settle(past);
			UASSERT(there.face != face);
			const bool acrossX = near(std::abs(there.x), h, 1e-5);
			UASSERT(acrossX || near(std::abs(there.z), h, 1e-5));
			const double other = acrossX ? there.z : there.x;
			UASSERT(near(std::abs(other), std::abs(along), 1e-5));
		}
	}
}

void TestSphere::testEightCorners()
{
	const int32_t h = PLANET.half();
	std::set<std::tuple<long, long, long>> corners;
	for (int face = 0; face < 6; face++)
	for (int sx : { -1, 1 })
	for (int sz : { -1, 1 }) {
		const Sphere::Vec d = PLANET.direction({ face, double(sx * h), double(sz * h) });
		corners.insert({ std::lround(d.x * 1e6), std::lround(d.y * 1e6), std::lround(d.z * 1e6) });
	}
	// 24 face corners, three to each point of the planet
	UASSERTEQ(size_t, corners.size(), 8);
}

void TestSphere::testCellSides()
{
	// a metre along the centre lines
	for (double at : { -20000.0, 0.0, 15000.0 }) {
		const Sphere::Vec a = PLANET.point({ 0, at, 0 }, 0), b = PLANET.point({ 0, at + 1, 0 }, 0);
		UASSERT(near(std::hypot(b.x - a.x, b.y - a.y, b.z - a.z), 1.0, 1e-4));
	}
	// and never much less anywhere: what makes the corners livable
	const std::array<double, 2> sides = PLANET.cellSides();
	UASSERT(sides[0] > 0.7 && sides[1] < 1.0001);
}

void TestSphere::testFrame()
{
	for (const Sphere::Place &p : std::initializer_list<Sphere::Place>{ { 0, 0, 0 }, { 4, 20000, -9000 }, { 5, -29000, 29000 } }) {
		const Sphere::Frame f = PLANET.frame(p);
		const Sphere::Vec d = PLANET.direction(p);
		UASSERT(near(f.up.x, d.x, 1e-12) && near(f.up.y, d.y, 1e-12) && near(f.up.z, d.z, 1e-12));
		UASSERT(near(f.east.x * f.up.x + f.east.y * f.up.y + f.east.z * f.up.z, 0, 1e-9));
		UASSERT(near(f.north.x * f.up.x + f.north.y * f.up.y + f.north.z * f.up.z, 0, 1e-9));
	}
	const Sphere::Frame c = PLANET.frame({ 0, 0, 0 });
	UASSERT(near(c.east.z, 1, 1e-9) && near(c.north.y, 1, 1e-9));
	UASSERT(near(PLANET.latitude({ 4, 0, 0 }), M_PI / 2, 1e-12));
	UASSERT(near(PLANET.longitude({ 1, 0, 0 }), M_PI / 2, 1e-12));
}

// Anywhere on a face, next to the eye the lens is the engine's grid: a block
// east is a metre along x, a metre up is a metre along y.
void TestSphere::testLensIsTheGridAtTheEye()
{
	for (const Sphere::Place &eye : std::initializer_list<Sphere::Place>{ { 0, 0, 0 }, { 0, 21000, -26000 }, { 3, -29800, 29800 } })
	for (double height : { 0.0, 120.0, 25000.0 }) {
		const Sphere::Lens<double> lens(PLANET, eye, height);
		const auto at = [&](double e, double n, double hh) { return lens.at(e, n, hh); };
		const auto o = at(0, 0, height);
		UASSERT(near(o.x, 0, 1e-9) && near(o.y, 0, 1e-9) && near(o.z, 0, 1e-9));
		const auto e = at(0.01, 0, height), n = at(0, 0.01, height), u = at(0, 0, height + 0.01);
		UASSERT(near(e.x, 0.01, 1e-7) && near(e.y, 0, 1e-7) && near(e.z, 0, 1e-7));
		UASSERT(near(n.x, 0, 1e-7) && near(n.y, 0, 1e-7) && near(n.z, 0.01, 1e-7));
		UASSERT(near(u.x, 0, 1e-9) && near(u.y, 0.01, 1e-9) && near(u.z, 0, 1e-9));
		// straight down a column is straight down
		const auto down = at(0, 0, height - 50);
		UASSERT(near(down.x, 0, 1e-6) && near(down.y, -50, 1e-6) && near(down.z, 0, 1e-6));
	}
}

// From a face's centre on the sea, ground 1 km away falls R(1 − cos θ)
// below the eye's horizon, and the horizon itself is where it should be.
void TestSphere::testLensCurves()
{
	const Sphere::Lens<double> lens(PLANET, { 0, 0, 0 }, 0);
	const double R = PLANET.radius(), theta = 1000 / R;
	const auto p = lens.at(1000, 0, 0);
	UASSERT(near(p.y, -R * (1 - std::cos(theta)), 1e-6));
	UASSERT(near(p.x, R * std::sin(theta), 1e-6));
	UASSERT(p.y < -13 && p.y > -13.5); // ~13 m: visible from a hill, as on the planet
}

// What the shaders do: the same sums in floats. Within the view of any eye
// anywhere on a face, a block is where doubles put it to the millimetre.
void TestSphere::testLensInFloats()
{
	for (const Sphere::Place &eye : std::initializer_list<Sphere::Place>{ { 0, 0, 0 }, { 0, 21000.5, -26000.25 }, { 2, -29800, 29800 } })
	for (double height : { 2.0, 9000.0 }) {
		const Sphere::Lens<double> d(PLANET, eye, height);
		const Sphere::Lens<float> f(PLANET, eye, height);
		for (double e : { -600.0, -3.5, 0.5, 47.0, 900.0 })
		for (double n : { -800.0, -1.0, 0.0, 12.5, 650.0 })
		for (double h : { -20.0, 0.0, 64.0 }) {
			const auto a = d.at(e, n, h);
			const auto b = f.at(float(e), float(n), float(h));
			UASSERT(near(a.x, b.x, 2e-3) && near(a.y, b.y, 2e-3) && near(a.z, b.z, 2e-3));
		}
		// the far view's reach, out to the face's far side: within a part in 10⁵
		for (double e : { -25000.0, -7000.0, 12000.0 })
		for (double n : { -20000.0, 3000.0, 25000.0 }) {
			const auto a = d.at(e, n, 30);
			const auto b = f.at(float(e), float(n), 30.0f);
			const double far = std::hypot(a.x, a.y, a.z);
			UASSERT(near(a.x, b.x, 1e-5 * far) && near(a.y, b.y, 1e-5 * far) && near(a.z, b.z, 1e-5 * far));
		}
	}
}
