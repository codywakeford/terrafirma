// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "test.h"

#include "ground/ground.h"
#include "collision.h"
#include "dummygamedef.h"
#include "dummymap.h"
#include "environment.h"
#include "object_properties.h"
#include "network/networkprotocol.h"
#include "noise.h"
#include <sstream>
#include <map>
#include <memory>

using namespace ground;

class TestGround : public TestBase {
public:
	TestGround() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestGround"; }

	void runTests(IGameDef *gamedef);

	void testFlatStaysFlat();
	void testAStepIsARamp();
	void testTwoBlocksIsACliff();
	void testSolidKeepsItsShape();
	void testNeighboursAgree();
	void testTheClientKnowsLoose();
	void testUnknownAndCubesAreWalls();
	void testWalksUpASteppedHill();
	void testFirstCutKeepsTheShape();
};

static TestGround g_test_instance;

void TestGround::runTests(IGameDef *gamedef)
{
	TEST(testFlatStaysFlat);
	TEST(testAStepIsARamp);
	TEST(testTwoBlocksIsACliff);
	TEST(testSolidKeepsItsShape);
	TEST(testNeighboursAgree);
	TEST(testTheClientKnowsLoose);
	TEST(testUnknownAndCubesAreWalls);
	TEST(testWalksUpASteppedHill);
	TEST(testFirstCutKeepsTheShape);
}

namespace {

// Columns of ground: each a surface level and what it's made of; air above.
struct Land
{
	std::unique_ptr<NodeDefManager> ndef{ createNodeDefManager() };
	content_t dirt, stone;
	std::map<std::pair<int, int>, std::pair<int, bool>> columns; // (x, z) → (top, solid)
	int rest = 0; // the surface beyond the columns set

	Land()
	{
		ContentFeatures d;
		d.name = "test:dirt";
		d.walkable = true;
		d.groups["loose"] = 1;
		dirt = ndef->set(d.name, std::move(d));
		ContentFeatures s;
		s.name = "test:stone";
		s.walkable = true;
		stone = ndef->set(s.name, std::move(s));
	}

	void set(int x, int z, int top, bool solid = false) { columns[{ x, z }] = { top, solid }; }

	MapNode operator()(v3s16 p) const
	{
		auto it = columns.find({ p.X, p.Z });
		int top = it == columns.end() ? rest : it->second.first;
		bool solid = it != columns.end() && it->second.second;
		if (p.Y >= top)
			return MapNode(CONTENT_AIR);
		return MapNode(solid ? stone : dirt);
	}

	// the smoothed top of the column at (x, z)
	std::optional<Top> top(int x, int z) const
	{
		auto it = columns.find({ x, z });
		int t = it == columns.end() ? rest : it->second.first;
		return Ground(*ndef).top(*this, v3s16(x, t - 1, z));
	}
};

} // namespace

void TestGround::testFlatStaysFlat()
{
	Land land;
	auto t = land.top(0, 0);
	UASSERT(t);
	for (int i = 0; i < 4; i++) {
		UASSERTEQ(int, t->corners[i], STEPS);
		UASSERTEQ(int, t->sides[i][0], STEPS);
		UASSERTEQ(int, t->sides[i][1], STEPS);
	}
	// not a top: ground over it
	UASSERT(!Ground(*land.ndef).top(land, v3s16(0, -2, 0)));
}

void TestGround::testAStepIsARamp()
{
	// a step up of one block at x = 1, along z
	Land land;
	for (int x = 1; x <= 8; x++)
		for (int z = -8; z <= 8; z++)
			land.set(x, z, 1);
	auto low = land.top(0, 0), high = land.top(1, 0);
	UASSERT(low && high);
	// the step's edge, at x = 1: halfway up, seen from both sides
	UASSERTEQ(int, low->corners[1], STEPS + STEPS / 2);
	UASSERTEQ(int, low->corners[3], STEPS + STEPS / 2);
	UASSERTEQ(int, high->corners[0], STEPS / 2);
	UASSERTEQ(int, high->corners[2], STEPS / 2);
	// the far edges as they were
	UASSERTEQ(int, low->corners[0], STEPS);
	UASSERTEQ(int, high->corners[1], STEPS);
	// the surface is one ramp: no wall either side
	UASSERTEQ(int, high->sides[0][0], high->corners[0]);
	UASSERTEQ(int, low->sides[1][1], low->corners[3]);
	// halfway across the high block, three quarters up
	UASSERT(std::fabs(high->height(0.5f, 0.5f) - 0.75f) < 1e-4f);
}

void TestGround::testTwoBlocksIsACliff()
{
	Land land;
	for (int x = 1; x <= 8; x++)
		for (int z = -8; z <= 8; z++)
			land.set(x, z, 2);
	auto low = land.top(0, 0), high = land.top(1, 0);
	UASSERT(low && high);
	// neither leans toward the other
	UASSERTEQ(int, low->corners[1], STEPS);
	UASSERTEQ(int, high->corners[0], STEPS);
	// the high side's wall reaches down to (into) the low side's ground
	UASSERT(high->sides[0][0] <= -STEPS);
}

void TestGround::testSolidKeepsItsShape()
{
	// stone at the same height beside dirt, lower ground the other way
	Land land;
	land.set(1, 0, 0, true);
	land.set(-1, 0, -1);
	land.set(-1, 1, -1);
	land.set(-1, -1, -1);
	land.set(0, -1, -1);
	land.set(1, -1, -1);
	UASSERT(!land.top(1, 0)); // stone isn't smoothed
	auto t = land.top(0, 0);
	UASSERT(t);
	// beside the stone the dirt stays full, though the corner's ground
	// averages lower: the stone's face against it stays hidden
	UASSERTEQ(int, t->corners[1], STEPS);
	UASSERTEQ(int, t->corners[3], STEPS);
	// toward the low side it leans down
	UASSERT(t->corners[0] < STEPS);
}

/*
	A rough field of dirt with some stone: for every smoothed column, what
	it says of each neighbour's edge is what that neighbour draws there,
	where it's within a block (so the surface has no cracks between them);
	and it never falls under its own surface beside anything that would
	have hidden a face against it.
*/
void TestGround::testNeighboursAgree()
{
	Land land;
	const int N = 24;
	for (int x = -N; x <= N; x++)
		for (int z = -N; z <= N; z++) {
			float n = noise2d_value(x / 5.f, z / 5.f, 7) + 0.3f * noise2d(x, z, 11);
			int top = int(std::floor(n * 3));
			land.set(x, z, top, noise2d(x * 5, z * 5, 13) > 0.7f);
		}
	int checked = 0, lowered = 0, raised = 0;
	for (int x = -N + 4; x <= N - 4; x++)
		for (int z = -N + 4; z <= N - 4; z++) {
			auto a = land.top(x, z);
			if (!a)
				continue;
			int ta = land.columns[{ x, z }].first;
			for (int i = 0; i < 4; i++) {
				lowered += a->corners[i] < STEPS;
				raised += a->corners[i] > STEPS;
			}
			for (int s = 0; s < 4; s++) {
				int nx = x + SIDE_DIR[s][0], nz = z + SIDE_DIR[s][1];
				auto [tb, solid] = land.columns[{ nx, nz }];
				for (int k = 0; k < 2; k++) {
					int i = SIDE_CORNERS[s][k];
					int mine = a->corners[i] + (ta - 1) * STEPS;
					int theirs = a->sides[s][k] + (ta - 1) * STEPS;
					if (tb > ta || (tb == ta && solid))
						UASSERT(mine >= ta * STEPS);
					if (solid) {
						UASSERTEQ(int, theirs, tb * STEPS);
						continue;
					}
					if (std::abs(tb - ta) >= 2)
						continue;
					auto b = land.top(nx, nz);
					UASSERT(b);
					// their corner at this same world corner
					int j = (i & 1) - SIDE_DIR[s][0] + 2 * ((i >> 1) - SIDE_DIR[s][1]);
					UASSERTEQ(int, theirs, b->corners[j] + (tb - 1) * STEPS);
					checked++;
				}
			}
		}
	// the field was rough enough to mean something
	UASSERT(checked > 1000);
	UASSERT(lowered > 100);
	UASSERT(raised > 100);
}

// The client's definitions come from the server's, sent: loose with them
// (it's derived, not sent).
void TestGround::testTheClientKnowsLoose()
{
	Land land;
	std::ostringstream os(std::ios::binary);
	land.ndef->serialize(os, LATEST_PROTOCOL_VERSION);
	std::unique_ptr<NodeDefManager> client(createNodeDefManager());
	std::istringstream is(os.str(), std::ios::binary);
	client->deSerialize(is, LATEST_PROTOCOL_VERSION);
	UASSERT(client->get(land.dirt).loose);
	UASSERT(!client->get(land.stone).loose);
	UASSERT(Ground(*client).kind(MapNode(land.dirt)) == Kind::Loose);
}

namespace {

class HillEnvironment : public Environment {
	DummyMap map;
public:
	HillEnvironment(IGameDef *gamedef) : Environment(gamedef), map(gamedef, {-1, -1, -1}, {1, 1, 1}) {}
	void step(f32 dtime) override {}
	Map &getMap() override { return map; }
	void getSelectedActiveObjects(const core::line3d<f32> &, std::vector<PointedThing> &,
			const std::optional<Pointabilities> &) override {}
};

} // namespace

/*
	A player-sized mover walks up a hill of dirt that rises a whole block
	every three: a cube's step is more than it can step up (0.6), smoothed
	ground's voxels aren't. Through the collision the player moves by, the
	whole way never inside the ground. With the ground stood on as cubes it
	stops at the first step.
*/
void TestGround::testWalksUpASteppedHill()
{
	DummyGameDef gamedef;
	NodeDefManager *ndef = gamedef.getWritableNodeDefManager();
	ContentFeatures d;
	d.name = "test:dirt";
	d.walkable = true;
	d.groups["loose"] = 1;
	const content_t dirt = ndef->set(d.name, std::move(d));

	HillEnvironment env(&gamedef);
	Map &map = env.getMap();
	auto top = [](int x) { return 2 + int(std::floor(x / 3.f)); };
	for (s16 x = -16; x < 32; x++)
		for (s16 z = -16; z < 32; z++)
			for (s16 y = -16; y < 32; y++)
				map.setNode(v3s16(x, y, z), MapNode(y < top(x) ? dirt : CONTENT_AIR));

	const aabb3f box(-0.3f * BS, 0, -0.3f * BS, 0.3f * BS, 1.77f * BS, 0.3f * BS);
	v3f pos(-10 * BS, (top(-10) + 0.5f) * BS, 0);
	v3f speed;
	bool touching = false;
	for (int i = 0; i < 400 && pos.X < 24 * BS; i++) {
		speed.X = 4 * BS;
		speed.Z = 0;
		const f32 stepheight = touching ? 0.6f * BS : 0.2f * BS; // as the local player's
		CollisionMoveResult r = collisionMoveSimple(&env, &gamedef, box, stepheight, 0.05f,
				&pos, &speed, v3f(0, -9.81f * BS, 0), nullptr, false, StepUpMode::LEGACY);
		touching = r.touching_ground;
		UASSERT(!collision_check_intersection(&env, &gamedef, box, pos, nullptr, false));
	}
	UTEST(pos.X >= 24 * BS, "stopped at x %.2f", pos.X / BS);
	// and stands on it there: its feet at the voxels under its middle, or a
	// little above (its box is wider than a voxel)
	const int x = int(std::floor(pos.X / BS + 0.5f));
	auto t = Ground(*ndef).top([&](v3s16 p) { return map.getNode(p); }, v3s16(x, top(x) - 1, 0));
	UASSERT(t);
	const float under = top(x) - 1.5f + t->height(pos.X / BS + 0.5f - x, 0.5f);
	const float feet = pos.Y / BS;
	UTEST(feet >= under - 1e-3f && feet < under + 0.3f, "feet %.3f, ground %.3f", feet, under);
}

/*
	Beside a column not all known (its block not here yet) or a cell that
	keeps its cube (an atlas's cut cell), the ground isn't lowered: either
	may be a full block whose face against it is hidden. Ground falling away
	on the far side, so the corners there would otherwise lean down.
*/
void TestGround::testUnknownAndCubesAreWalls()
{
	Land land;
	for (int z = -1; z <= 1; z++) {
		land.set(-1, z, -1);
		land.set(0, z, 0);
		land.set(1, z, 0);
	}
	land.set(0, -1, -1);
	land.set(1, -1, -1);
	const Ground ground(*land.ndef);
	// as it is: the corner at (1, 0) leans down toward the low row at z = −1
	auto open = ground.top(land, v3s16(0, -1, 0));
	UASSERT(open && open->corners[1] < STEPS);

	// the column at (1, 0) unknown
	auto unknown = [&](v3s16 p) {
		return (p.X == 1 && p.Z == 0) ? MapNode(CONTENT_IGNORE) : land(p);
	};
	auto t = ground.top(unknown, v3s16(0, -1, 0));
	UASSERT(t);
	UASSERTEQ(int, t->corners[1], STEPS);
	UASSERTEQ(int, t->corners[3], STEPS);

	// the column at (1, 0) a cube (cut)
	auto cube = [](v3s16 p) { return p.X == 1 && p.Z == 0; };
	t = ground.top(land, cube, v3s16(0, -1, 0));
	UASSERT(t);
	UASSERTEQ(int, t->corners[1], STEPS);
	UASSERTEQ(int, t->corners[3], STEPS);
	// and a cube itself isn't smoothed
	UASSERT(!ground.top(land, cube, v3s16(1, -1, 0)));
}

/*
	Smoothed ground cut the first time (Map::settleGround) keeps the shape
	it had, its own cell and what rose into the one above: what's stood on
	there is the same; and the ground round it reads it as before, so its
	shape doesn't change either.
*/
void TestGround::testFirstCutKeepsTheShape()
{
	DummyGameDef gamedef;
	NodeDefManager *ndef = gamedef.getWritableNodeDefManager();
	ContentFeatures d;
	d.name = "test:dirt";
	d.walkable = true;
	d.drawtype = NDT_NORMAL;
	d.groups["loose"] = 1;
	const content_t dirt = ndef->set(d.name, std::move(d));

	HillEnvironment env(&gamedef);
	Map &map = env.getMap();
	auto top = [](int x, int z) { return 4 + int(std::floor((x + z / 2) / 3.f)); };
	for (s16 x = -16; x < 32; x++)
		for (s16 z = -16; z < 32; z++)
			for (s16 y = -16; y < 32; y++)
				map.setNode(v3s16(x, y, z), MapNode(y < top(x, z) ? dirt : CONTENT_AIR));

	// what's stood on in a column of cells, as sixteenth columns (x, z) → height
	auto stood = [&](v3s16 p) {
		std::map<std::pair<int, int>, int> h;
		for (s16 dy = 0; dy < 2; dy++) {
			std::vector<aabb3f> boxes;
			const v3s16 q = p + v3s16(0, dy, 0);
			if (!map.getShapedBoxes(q, boxes) && map.getNode(q).getContent() == dirt)
				boxes.emplace_back(-0.5f * BS, -0.5f * BS, -0.5f * BS, 0.5f * BS, 0.5f * BS, 0.5f * BS);
			for (const aabb3f &b : boxes)
				for (int x = 0; x < STEPS; x++)
					for (int z = 0; z < STEPS; z++) {
						const v3f mid((x + 0.5f) / STEPS - 0.5f, 0, (z + 0.5f) / STEPS - 0.5f);
						if (mid.X * BS > b.MinEdge.X && mid.X * BS < b.MaxEdge.X
								&& mid.Z * BS > b.MinEdge.Z && mid.Z * BS < b.MaxEdge.Z) {
							int &v = h[{ x, z }];
							v = std::max(v, dy * STEPS + int(std::lround((b.MaxEdge.Y / BS + 0.5f) * STEPS)));
						}
					}
		}
		return h;
	};

	int risen = 0;
	for (s16 x = 2; x < 12; x++) {
		const v3s16 p(x, top(x, 0) - 1, 0);
		const std::optional<Top> was = map.groundTop(p);
		UASSERT(was);
		risen += !was->shape(1).empty();
		const auto before = stood(p);
		std::map<v3s16, std::optional<Top>> around;
		for (s16 dx = -2; dx <= 2; dx++)
			for (s16 dz = -2; dz <= 2; dz++)
				if (dx || dz) {
					const v3s16 q(x + dx, top(x + dx, dz) - 1, dz);
					around[q] = map.groundTop(q);
				}

		map.settleGround(p);
		// nothing's left to the rule that the chisel would start from as a
		// cube: smoothed still only where it was a cube and nothing rose
		if (const std::optional<Top> still = map.groundTop(p))
			UASSERT(still->shape(0).isWhole() && still->shape(1).empty());
		UASSERT(stood(p) == before);
		for (const auto &[q, t] : around) {
			const std::optional<Top> now = map.groundTop(q);
			UASSERT(now.has_value() == t.has_value());
			if (t)
				UASSERT(now->cells == t->cells);
		}
	}
	UASSERT(risen > 0); // some rose into the cell above
}
