// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "test.h"

#include "constants.h"
#include "planet/planet.h"
#include <cmath>
#include <map>
#include <set>

using namespace planet;

class TestPlanet : public TestBase {
public:
	TestPlanet() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestPlanet"; }

	void runTests(IGameDef *gamedef);

	void testNetFillsTheMap();
	void testFaces();
	void testNearest();
	void testPlaceRoundTrip();
	void testStepWithinAFace();
	void testStepAcrossEverySide();
	void testStepsAreNeighbours();
	void testEdgesPairUp();
	void testCornersClose();
	void testTurnFollowsTheGround();
};

static TestPlanet g_test_instance;

void TestPlanet::runTests(IGameDef *gamedef)
{
	TEST(testNetFillsTheMap);
	TEST(testFaces);
	TEST(testNearest);
	TEST(testPlaceRoundTrip);
	TEST(testStepWithinAFace);
	TEST(testStepAcrossEverySide);
	TEST(testStepsAreNeighbours);
	TEST(testEdgesPairUp);
	TEST(testCornersClose);
	TEST(testTurnFollowsTheGround);
}

static const Planet FULL(Planet::MAX_HALF);
static const Planet SMALL(64); // small enough to walk every edge node

static const int DIRS[4][2] = { { 1, 0 }, { 0, 1 }, { -1, 0 }, { 0, -1 } };

// Every node along a face's side `d` (one of DIRS): the last row before the edge.
template <typename F>
static void alongSide(const Planet &p, int face, int d, F f)
{
	const Planet::Square s = p.square(face);
	const int32_t w = p.width();
	for (int32_t i = 0; i < w; i++) {
		int32_t x = s.x + i, z = s.z + i;
		if (DIRS[d][0] != 0)
			x = DIRS[d][0] > 0 ? s.x + w - 1 : s.x;
		else
			z = DIRS[d][1] > 0 ? s.z + w - 1 : s.z;
		f(x, z);
	}
}

void TestPlanet::testNetFillsTheMap()
{
	// the largest planet: from the map's lowest whole block to its highest node
	UASSERTEQ(int, Planet::ORIGIN, -(MAX_MAP_GENERATION_LIMIT / MAP_BLOCKSIZE) * MAP_BLOCKSIZE);
	UASSERTEQ(int, FULL.square(2).x + FULL.width() - 1, MAX_MAP_GENERATION_LIMIT);
	UASSERT(FULL.square(5).z + FULL.width() - 1 < MAX_MAP_GENERATION_LIMIT);
	// faces start on map blocks, so no block is on two
	for (int f = 0; f < 6; f++) {
		UASSERTEQ(int, ((FULL.square(f).x % MAP_BLOCKSIZE) + MAP_BLOCKSIZE) % MAP_BLOCKSIZE, 0);
		UASSERTEQ(int, ((FULL.square(f).z % MAP_BLOCKSIZE) + MAP_BLOCKSIZE) % MAP_BLOCKSIZE, 0);
	}
	EXCEPTION_CHECK(std::invalid_argument, Planet(Planet::MAX_HALF + 8));
	EXCEPTION_CHECK(std::invalid_argument, Planet(100)); // not whole map blocks
}

void TestPlanet::testFaces()
{
	for (const Planet *p : { &FULL, &SMALL })
	for (int f = 0; f < 6; f++) {
		const Planet::Square s = p->square(f);
		const int32_t w = p->width();
		UASSERT(p->face(s.x, s.z) == f && p->face(s.x + w - 1, s.z + w - 1) == f);
		// the gaps and the edges of the net are no face's
		UASSERT(!p->face(s.x - 1, s.z).has_value() && !p->face(s.x, s.z - 1).has_value());
		UASSERT(!p->face(s.x + w, s.z).has_value());
		UASSERT(!p->face(s.x, s.z + w).has_value());
		UASSERT(!p->face(s.x + w + Planet::GAP - 1, s.z).has_value());
	}
}

void TestPlanet::testNearest()
{
	for (int f = 0; f < 6; f++) {
		const Planet::Square s = SMALL.square(f);
		const int32_t w = SMALL.width();
		for (int32_t z = s.z; z < s.z + w; z += 7)
		for (int32_t x = s.x; x < s.x + w; x += 7)
			UASSERTEQ(int, SMALL.nearest(x, z), f);
		// a vertex on the face's own boundary is its own
		UASSERTEQ(int, SMALL.nearest(s.x - 0.5, s.z - 0.5), f);
		UASSERTEQ(int, SMALL.nearest(s.x + w - 0.5, s.z + w - 0.5), f);
	}
}

void TestPlanet::testPlaceRoundTrip()
{
	for (int f = 0; f < 6; f++) {
		const Planet::Square s = FULL.square(f);
		const terragen::Sphere::Place p = FULL.place(f, s.x, s.z + FULL.width() - 1);
		UASSERT(p.x == -FULL.half() + 0.5 && p.z == FULL.half() - 0.5);
		const auto back = FULL.at(p);
		UASSERT(back[0] == s.x && back[1] == s.z + FULL.width() - 1);
	}
}

void TestPlanet::testStepWithinAFace()
{
	const Planet::Square s = FULL.square(4);
	const Planet::Step st = FULL.step(s.x + 10, s.z + 10, 0, 1);
	UASSERT(st.x == s.x + 10 && st.z == s.z + 11 && st.face == 4 && st.turn == 0);
	EXCEPTION_CHECK(std::invalid_argument, FULL.step(s.x - 1, s.z, 1, 0)); // in a gap
	EXCEPTION_CHECK(std::invalid_argument, FULL.step(s.x, s.z, 1, 1));
}

// Off every side of every face: onto another face's edge row, and back the
// way it came lands where it started.
void TestPlanet::testStepAcrossEverySide()
{
	for (const Planet *p : { &SMALL, &FULL })
	for (int f = 0; f < 6; f++)
	for (int d = 0; d < 4; d++) {
		const int step = p == &FULL ? 997 : 1;
		int n = 0;
		alongSide(*p, f, d, [&](int32_t x, int32_t z) {
			if (n++ % step != 0)
				return;
			const Planet::Step out = p->step(x, z, DIRS[d][0], DIRS[d][1]);
			UASSERT(out.face != f);
			UASSERT(p->face(out.x, out.z) == out.face);
			const auto in = Planet::turned(DIRS[d][0], DIRS[d][1], out.turn);
			UASSERT(p->face(out.x + in[0], out.z + in[1]) == out.face); // it came in at the edge
			UASSERT(!p->face(out.x - in[0], out.z - in[1]).has_value() ||
				p->face(out.x - in[0], out.z - in[1]) != out.face);
			const Planet::Step back = p->step(out.x, out.z, -in[0], -in[1]);
			UASSERT(back.x == x && back.z == z && back.face == f);
			UASSERTEQ(int, (back.turn + out.turn) % 4, 0);
		});
	}
}

// A step is a step on the planet too: the two blocks' centres a block apart.
void TestPlanet::testStepsAreNeighbours()
{
	for (int f = 0; f < 6; f++)
	for (int d = 0; d < 4; d++)
		alongSide(SMALL, f, d, [&](int32_t x, int32_t z) {
			const Planet::Step out = SMALL.step(x, z, DIRS[d][0], DIRS[d][1]);
			const auto a = SMALL.sphere().point(SMALL.place(f, x, z), 0);
			const auto b = SMALL.sphere().point(SMALL.place(out.face, out.x, out.z), 0);
			const double gap = std::hypot(a.x - b.x, a.y - b.y, a.z - b.z);
			UASSERT(gap > 0.69 && gap < 1.001);
		});
}

// The 24 sides pair up into the cube's 12 edges; a face never meets its opposite.
void TestPlanet::testEdgesPairUp()
{
	std::map<std::pair<int, int>, std::pair<int, int>> partner;
	for (int f = 0; f < 6; f++)
	for (int d = 0; d < 4; d++) {
		const Planet::Square s = FULL.square(f);
		const int32_t mid = s.x + FULL.half(), midz = s.z + FULL.half();
		const int32_t x = DIRS[d][0] > 0 ? s.x + FULL.width() - 1 : DIRS[d][0] < 0 ? s.x : mid;
		const int32_t z = DIRS[d][1] > 0 ? s.z + FULL.width() - 1 : DIRS[d][1] < 0 ? s.z : midz;
		const Planet::Step out = FULL.step(x, z, DIRS[d][0], DIRS[d][1]);
		const auto in = Planet::turned(DIRS[d][0], DIRS[d][1], out.turn);
		int back = 0;
		while (DIRS[back][0] != -in[0] || DIRS[back][1] != -in[1])
			back++;
		partner[{ f, d }] = { out.face, back };
		UASSERT(out.face != (f < 4 ? (f + 2) % 4 : 9 - f)); // 0↔2, 1↔3, 4↔5
	}
	UASSERTEQ(size_t, partner.size(), 24);
	for (const auto &[side, other] : partner)
		UASSERT(partner.at(other) == side);
	for (int f = 0; f < 6; f++) {
		std::set<int> faces;
		for (int d = 0; d < 4; d++)
			faces.insert(partner.at({ f, d }).first);
		UASSERTEQ(size_t, faces.size(), 4);
	}
}

// Round a corner: off a face's corner block east lands on one neighbour,
// north on another, and those two are next to each other: three blocks round
// the point, not four.
void TestPlanet::testCornersClose()
{
	std::set<std::set<int>> corners;
	for (const Planet *p : { &SMALL, &FULL })
	for (int f = 0; f < 6; f++)
	for (int sx : { -1, 1 })
	for (int sz : { -1, 1 }) {
		const Planet::Square s = p->square(f);
		const int32_t x = sx > 0 ? s.x + p->width() - 1 : s.x, z = sz > 0 ? s.z + p->width() - 1 : s.z;
		const Planet::Step a = p->step(x, z, sx, 0), b = p->step(x, z, 0, sz);
		UASSERT(a.face != b.face);
		// from a, the way that was north on f (turned): round the corner to b
		const auto round = Planet::turned(0, sz, a.turn);
		const Planet::Step c = p->step(a.x, a.z, round[0], round[1]);
		UASSERT(c.face == b.face && c.x == b.x && c.z == b.z);
		if (p == &FULL)
			corners.insert({ f, a.face, b.face });
	}
	UASSERTEQ(size_t, corners.size(), 8);
}

// A turn is what the ground does: the way across the edge, and the way
// along it, point the same way on the planet on both sides of it.
void TestPlanet::testTurnFollowsTheGround()
{
	const auto direction = [](const Planet &p, int face, int32_t x, int32_t z, int dx, int dz) {
		const terragen::Sphere::Frame fr = p.sphere().frame(p.place(face, x, z));
		return terragen::Sphere::Vec{ dx * fr.east.x + dz * fr.north.x, dx * fr.east.y + dz * fr.north.y,
			dx * fr.east.z + dz * fr.north.z };
	};
	const auto dot = [](terragen::Sphere::Vec a, terragen::Sphere::Vec b) { return a.x * b.x + a.y * b.y + a.z * b.z; };
	for (int f = 0; f < 6; f++)
	for (int d = 0; d < 4; d++) {
		int n = 0;
		alongSide(SMALL, f, d, [&](int32_t x, int32_t z) {
			if (n++ % 9 != 4)
				return;
			const Planet::Step out = SMALL.step(x, z, DIRS[d][0], DIRS[d][1]);
			for (const auto &way : { DIRS[d], DIRS[(d + 1) % 4] }) { // across, and along
				const auto there = Planet::turned(way[0], way[1], out.turn);
				const double same = dot(direction(SMALL, f, x, z, way[0], way[1]),
					direction(SMALL, out.face, out.x, out.z, there[0], there[1]));
				UASSERT(same > 0.3); // the grids skew up to 60° at corners (cos = 0.5); a wrong turn is −1
			}
		});
	}
}
