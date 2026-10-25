// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "test.h"

#include "constants.h"
#include "mapgen/mapgen.h"
#include "planet/planet.h"
#include "planet/flying.h"
#include "terragen/planet.h"
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
	void testLensIsThePlanet();
	void testLensAcrossEdges();
	void testLensInFloats();
	void testWrap();
	void testDistance();
	void testHalo();
	void testCarry();
	void testFlyingRoundTrip();
	void testFlyingInOrbit();
	void testCorner();
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
	TEST(testLensIsThePlanet);
	TEST(testLensAcrossEdges);
	TEST(testLensInFloats);
	TEST(testWrap);
	TEST(testDistance);
	TEST(testHalo);
	TEST(testCarry);
	TEST(testFlyingRoundTrip);
	TEST(testFlyingInOrbit);
	TEST(testCorner);
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
	// the largest planet: across all the map generator makes with default chunks
	const auto [emin, emax] = get_mapgen_edges(MAX_MAP_GENERATION_LIMIT, v3s16(5));
	UASSERTEQ(int, Planet::ORIGIN, emin.X);
	UASSERT(FULL.square(2).x + FULL.width() - 1 <= emax.X);
	UASSERT(FULL.square(2).x + FULL.width() + 2 * Planet::GAP > emax.X); // no room for a bigger one
	UASSERT(FULL.square(5).z + FULL.width() - 1 <= emax.Z);
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

static const Planet MID(512);

// Where a point is from the eye, the plain way (doubles, no care for
// precision): both points on the planet, the difference in the eye's frame.
static Lens<double>::V3 truth(const Planet &p, const Lens<double> &lens, double ex, double ey, double ez,
		double x, double y, double z)
{
	const int f = p.nearest(x, z);
	const auto a = p.sphere().point(p.place(f, x, z), y), e = p.sphere().point(p.place(lens.face, ex, ez), ey);
	const terragen::Sphere::Axes &ax = terragen::Sphere::axes(lens.face);
	const terragen::Sphere::Vec d{ a.x - e.x, a.y - e.y, a.z - e.z };
	const auto dot = [](terragen::Sphere::Vec u, terragen::Sphere::Vec v) { return u.x * v.x + u.y * v.y + u.z * v.z; };
	const double l[3] = { dot(d, ax.east), dot(d, ax.out), dot(d, ax.north) };
	const auto row = [&](int i) { const auto &w = lens.near.rows[i]; return w.x * l[0] + w.y * l[1] + w.z * l[2]; };
	return { row(0), row(1), row(2) };
}

// Eyes: a face's middle, near an edge, near a corner, and high up.
template <typename F>
static void eyes(const Planet &p, F f)
{
	const Planet::Square s = p.square(0);
	const double w = p.width();
	f(s.x + 0.5 * w, 12.0, s.z + 0.5 * w);
	f(s.x + w - 6.3, 3.0, s.z + 0.4 * w);
	f(s.x + w - 4.5, 20.0, s.z + w - 7.25);
	f(s.x + 0.1 * w, 400.0, s.z + 0.9 * w);
}

// On the eye's face and off it, the lens puts a point where the planet has it.
void TestPlanet::testLensIsThePlanet()
{
	for (const Planet *p : { &SMALL, &MID })
	eyes(*p, [&](double ex, double ey, double ez) {
		const Lens<double> lens(*p, ex, ey, ez);
		for (int f = 0; f < 6; f++) {
			const Planet::Square s = p->square(f);
			for (double fx : { 0.0, 0.01, 0.5, 0.99, (p->width() - 1.0) / p->width() })
			for (double fz : { 0.0, 0.02, 0.6, 0.98, (p->width() - 1.0) / p->width() }) {
				const double x = s.x + fx * p->width(), z = s.z + fz * p->width();
				const auto a = lens.at(x - ex, 5, z - ez), b = truth(*p, lens, ex, ey, ez, x, 5, z);
				UASSERT(std::abs(a.x - b.x) < 1e-6 && std::abs(a.y - b.y) < 1e-6 && std::abs(a.z - b.z) < 1e-6);
			}
		}
	});
}

// Either side of an edge, two blocks a step apart are as far apart as any
// two neighbours there, as the eye sees them: no crack, no overlap,
// wherever the eye is, on all the ground it can see.
void TestPlanet::testLensAcrossEdges()
{
	for (const Planet *p : { &SMALL, &MID })
	eyes(*p, [&](double ex, double ey, double ez) {
		// from the ground, where a crack would be seen (high over a tiny
		// planet the eye's skewed grid stretches the two steps differently)
		if (ey > 100)
			return;
		const Lens<double> lens(*p, ex, ey, ez);
		for (int f = 0; f < 6; f++)
		for (int d = 0; d < 4; d++) {
			int n = 0;
			alongSide(*p, f, d, [&](int32_t x, int32_t z) {
				if (n++ % 5 != 2)
					return;
				// only ground that faces the eye: the far side is out of sight
				const auto e = p->sphere().point(p->place(lens.face, ex, ez), ey);
				const auto g = p->sphere().point(p->place(f, x, z), 0);
				const auto up = p->sphere().direction(p->place(f, x, z));
				if ((e.x - g.x) * up.x + (e.y - g.y) * up.y + (e.z - g.z) * up.z <= 0)
					return;
				const Planet::Step out = p->step(x, z, DIRS[d][0], DIRS[d][1]);
				const auto a = lens.at(x - ex, 0, z - ez), b = lens.at(out.x - ex, 0, out.z - ez);
				const auto c = lens.at(x - DIRS[d][0] - ex, 0, z - DIRS[d][1] - ez); // the step before it
				const double across = std::hypot(a.x - b.x, a.y - b.y, a.z - b.z);
				const double inside = std::hypot(a.x - c.x, a.y - c.y, a.z - c.z);
				// the step over the edge is the size of the one before it (blocks
				// shrink and skew toward corners). A crack is a block or more;
				// testLensIsThePlanet holds every point to the planet itself.
				UASSERT(across > 0.5 * inside && across < 2 * inside);
			});
		}
	});
}

// What the shader does: the same sums in floats.
void TestPlanet::testLensInFloats()
{
	eyes(FULL, [&](double ex, double ey, double ez) {
		const Lens<double> d(FULL, ex, ey, ez);
		const Lens<float> f(FULL, ex, ey, ez);
		for (double e : { -700.0, -12.5, 0.5, 30.0, 650.0 })
		for (double n : { -500.0, 0.0, 9.5, 800.0 }) {
			const auto a = d.at(e, 40, n);
			const auto b = f.at(float(e), 40.0f, float(n));
			const double far = std::max(1.0, std::hypot(a.x, a.y, a.z));
			UASSERT(std::abs(a.x - b.x) < 3e-3 + 1e-5 * far && std::abs(a.y - b.y) < 3e-3 + 1e-5 * far
				&& std::abs(a.z - b.z) < 3e-3 + 1e-5 * far);
		}
	});
}

// A face's grid carried just past an edge lands where step() goes, and
// further on, near the planet's neighbouring point; on the face it's the node.
void TestPlanet::testWrap()
{
	for (int f = 0; f < 6; f++)
	for (int d = 0; d < 4; d++) {
		int n = 0;
		alongSide(MID, f, d, [&](int32_t x, int32_t z) {
			if (n++ % 31 != 3)
				return;
			UASSERT((MID.wrap(f, x, z) == std::array<int32_t, 2>{ x, z }));
			const Planet::Step out = MID.step(x, z, DIRS[d][0], DIRS[d][1]);
			UASSERT((MID.wrap(f, x + DIRS[d][0], z + DIRS[d][1]) == std::array<int32_t, 2>{ out.x, out.z }));
			// 20 blocks on: on the neighbour face, about 20 blocks from the edge
			const auto far = MID.wrap(f, x + 20 * DIRS[d][0], z + 20 * DIRS[d][1]);
			UASSERT(MID.face(far[0], far[1]) == out.face);
			const double gone = MID.distance(x, 0, z, far[0], 0, far[1]);
			UASSERT(gone > 14 && gone < 21.5);
		});
	}
}

void TestPlanet::testDistance()
{
	const Planet::Square s = MID.square(0);
	const double mid = s.x + MID.half() - 0.5;
	UASSERT(std::abs(MID.distance(mid, 0, mid, mid + 100, 0, mid) - 100) < 0.2); // a metre a block, mid-face
	UASSERT(std::abs(MID.distance(mid, 0, mid, mid, 50, mid) - 50) < 1e-9);       // straight up
	// across an edge: a step is a block
	const Planet::Step out = MID.step(s.x + MID.width() - 1, s.z + 300, 1, 0);
	UASSERT(MID.distance(s.x + MID.width() - 1, 0, s.z + 300, out.x, 0, out.z) < 1.01);
	// the far side: a diameter
	const Planet::Square o = MID.square(2);
	const double r = MID.sphere().radius();
	UASSERT(std::abs(MID.distance(mid, 0, mid, o.x + MID.half() - 0.5, 0, o.z + MID.half() - 0.5) - 2 * r) < 1e-6);
}

// Past an edge the halo is the neighbour face, exactly: a step from a halo
// node is the halo's next node; it's where the planet has it; and round a
// corner there's nothing.
void TestPlanet::testHalo()
{
	for (int f = 0; f < 6; f++)
	for (int d = 0; d < 4; d++) {
		int n = 0;
		alongSide(SMALL, f, d, [&](int32_t x, int32_t z) {
			if (n++ % 7 != 3)
				return;
			UASSERT(SMALL.halo(f, x, z)->face == f);
			std::optional<Planet::Step> prev = Planet::Step{ x, z, f, 0 };
			for (int k = 1; k <= 20; k++) {
				const auto h = SMALL.halo(f, x + k * DIRS[d][0], z + k * DIRS[d][1]);
				UASSERT(h && h->face != f && SMALL.face(h->x, h->z) == h->face);
				// one step on from the last one
				const auto way = Planet::turned(DIRS[d][0], DIRS[d][1], prev->turn);
				const Planet::Step next = SMALL.step(prev->x, prev->z, way[0], way[1]);
				UASSERT(next.x == h->x && next.z == h->z && next.face == h->face);
				// and as far from the edge as it should be, on the planet
				const double gone = SMALL.distance(x, 0, z, h->x, 0, h->z);
				UASSERT(gone > 0.69 * k && gone < 1.01 * k);
				prev = h;
			}
		});
	}
	// round a corner: nothing
	const Planet::Square s = SMALL.square(3);
	UASSERT(!SMALL.halo(3, s.x - 1, s.z - 1));
	UASSERT(!SMALL.halo(3, s.x + SMALL.width() + 4, s.z + SMALL.width() + 2));
}

// Off an edge by a fraction of a node: on the neighbour face, as far past
// its edge as it was past this one, at the same place along it; and the
// way it was going turns with the grid. On the face: not carried.
void TestPlanet::testCarry()
{
	for (int f = 0; f < 6; f++)
	for (int d = 0; d < 4; d++) {
		int n = 0;
		alongSide(SMALL, f, d, [&](int32_t x, int32_t z) {
			if (n++ % 9 != 4)
				return;
			UASSERT(!SMALL.carry(f, x + 0.3, z - 0.2)); // still on the face
			// just over the edge (it's carried the moment it crosses), a little along it
			const double ox = x + DIRS[d][0] * 0.51 + DIRS[d][1] * 0.21, oz = z + DIRS[d][1] * 0.51 + DIRS[d][0] * 0.21;
			const auto c = SMALL.carry(f, ox, oz);
			UASSERT(c && c->face != f && SMALL.face((int32_t)std::lround(c->x), (int32_t)std::lround(c->z)) == c->face);
			const Planet::Step s = SMALL.step(x, z, DIRS[d][0], DIRS[d][1]);
			UASSERT(c->turn == s.turn);
			// the same point on the planet, from either face
			const auto a = SMALL.sphere().point(SMALL.place(f, ox, oz), 0); // f's grid carried on
			const auto b = SMALL.sphere().point(SMALL.place(c->face, c->x, c->z), 0);
			UASSERT(std::hypot(a.x - b.x, a.y - b.y, a.z - b.z) < 0.01); // exact on the edge; the grids part only beyond it
			// carried back across, it's where it was
			const auto back = SMALL.carry(c->face, c->x - Planet::turned(DIRS[d][0], DIRS[d][1], c->turn)[0] * 1.24,
				c->z - Planet::turned(DIRS[d][0], DIRS[d][1], c->turn)[1] * 1.24);
			UASSERT(back && back->face == f && (back->turn + c->turn) % 4 == 0);
		});
	}
	const auto t = Planet::turned(1.0, 0.0, 1);
	UASSERT(t[0] == 0.0 && t[1] == 1.0);
}

static const Planet DEFAULT(2048); // a new world's

void TestPlanet::testFlyingRoundTrip()
{
	const orbit::Flight flight(terragen::sphere(DEFAULT.sphere().radius()));
	const Flying fly(DEFAULT, flight);
	for (int f = 0; f < 6; f++) {
		const Planet::Square s = DEFAULT.square(f);
		const Flying::Local l{ s.x + 701.25, s.z + 3333.5, 412.0, 3.0, -1.5, 7.0 };
		const Flying::Local b = fly.fromOrbit(fly.toOrbit(l));
		UASSERT(std::abs(b.x - l.x) < 1e-6 && std::abs(b.z - l.z) < 1e-6 && std::abs(b.altitude - l.altitude) < 1e-6);
		UASSERT(std::abs(b.east - l.east) < 1e-9 && std::abs(b.up - l.up) < 1e-9 && std::abs(b.north - l.north) < 1e-9);
	}
}

// Once round a circular orbit in the map's own terms: the height holds, and
// it crosses the four faces of the equator in turn, east.
void TestPlanet::testFlyingInOrbit()
{
	const terragen::Planet body = terragen::sphere(DEFAULT.sphere().radius());
	const orbit::Flight flight(body);
	const Flying fly(DEFAULT, flight);
	const double h = 2500, r = body.radius + h;
	const double ground = std::sqrt(body.mu() / r) - body.spin() * r; // circular, over the turning ground
	const Planet::Square s = DEFAULT.square(0);
	Flying::Local l{ s.x + DEFAULT.half() - 0.5, s.z + DEFAULT.half() - 0.5, h, ground, 0, 0 };
	const double period = flight.elements(fly.toOrbit(l)).period;
	const orbit::Craft craft{ 1000, 0 };
	std::vector<int> faces{ 0 };
	int turns = 0;
	for (double t = 0; t < period; t += 0.5) {
		const Flying::Step st = fly.step(l, craft, { 0, 0, 0 }, 0.5);
		l = st.where;
		turns += st.turn;
		UASSERT(std::abs(l.altitude - h) < 2.0);
		const int f = DEFAULT.nearest(l.x, l.z);
		if (f != faces.back())
			faces.push_back(f);
	}
	// against the stars it's gone round once; over the turning ground a little less
	UASSERT(faces.size() >= 4);
	UASSERT(faces[0] == 0 && faces[1] == 1 && faces[2] == 2 && faces[3] == 3);
	UASSERTEQ(int, turns % 4, 0); // the equator's edges don't turn the grid
}

// Every face's corner node is beside a corner; a face's middle is as far as can be.
void TestPlanet::testCorner()
{
	for (int f = 0; f < 6; f++) {
		const Planet::Square s = MID.square(f);
		const int32_t w = MID.width();
		for (const auto &c : { std::array<int32_t, 2>{ s.x, s.z }, { s.x + w - 1, s.z }, { s.x, s.z + w - 1 }, { s.x + w - 1, s.z + w - 1 } })
			UASSERT(MID.corner(c[0], c[1]) < 1.0);
		const double middle = MID.corner(s.x + MID.half() - 0.5, s.z + MID.half() - 0.5);
		UASSERT(std::abs(middle - std::acos(1 / std::sqrt(3.0)) * MID.sphere().radius()) < 1e-6);
	}
}
