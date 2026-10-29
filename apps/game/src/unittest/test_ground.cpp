// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "test.h"

#include "ground/ground.h"
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
