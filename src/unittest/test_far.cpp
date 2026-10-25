// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "test.h"

#include "far/horizon.h"
#include "far/terrain.h"
#include "planet/planet.h"
#include "network/networkpacket.h"
#include "network/networkprotocol.h"
#include "util/pointer.h"

using namespace far;

class TestFar : public TestBase {
public:
	TestFar() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestFar"; }

	void runTests(IGameDef *gamedef);

	void testPacketRoundTrip();
	void testTileMatchesColumns();
	void testCoarseCellsSampleCentres();
	void testPolarWall();
	void testWholeMap();
	void testOnThePlanet();
};

static TestFar g_test_instance;

void TestFar::runTests(IGameDef *gamedef)
{
	TEST(testPacketRoundTrip);
	TEST(testTileMatchesColumns);
	TEST(testCoarseCellsSampleCentres);
	TEST(testPolarWall);
	TEST(testWholeMap);
	TEST(testOnThePlanet);
}

// A horizon with made-up block ids: the far view only compares them.
static Horizon horizon()
{
	Horizon h;
	h.code = "TF5.dGVycmE";
	h.water_level = 1;
	terra::Blocks &b = h.blocks;
	content_t next = 100;
	for (terra::BiomeBlocks &bb : b.biomes)
		bb = { next++, next++, next++, 11, 1, 3, 1, 0 };
	b.stone = 10;
	b.water = 11;
	b.ice = 12;
	b.lava = 13;
	b.placer = 14;
	b.soil = 15;
	for (content_t &c : b.rock)
		c = next++;
	b.ore[0] = CONTENT_IGNORE;
	for (size_t i = 1; i < std::size(b.ore); i++)
		b.ore[i] = next++;
	terra::Species oak{ "oak", { { -1, 1 }, { -1, 1 }, false, { 0, 0 }, false, false }, {}, 20, 21,
		{ "FFFFA", "[&&fff][++&&fff][----&&fff]", "", "", "", "single", 45, 1, 0, true } };
	for (const terra::BiomeBlocks &bb : b.biomes)
		oak.on.push_back(bb.top);
	h.species.push_back(oak);
	return h;
}

void TestFar::testPacketRoundTrip()
{
	Horizon h = horizon();
	h.species[0].niche.heat[0] = -0.123456789012345; // exact bits, not a rounding
	NetworkPacket out(TOCLIENT_HORIZON, 0);
	write(out, h);
	// as it travels: the command, then the body
	Buffer<u8> wire = out.oldForgePacket();
	NetworkPacket in;
	in.putRawPacket(*wire, wire.getSize(), 0);
	Horizon r = read(in);
	UASSERT(r.code == h.code && r.water_level == h.water_level);
	UASSERT(r.blocks.biomes[3].filler == h.blocks.biomes[3].filler);
	UASSERT(r.blocks.rock[2] == h.blocks.rock[2] && r.blocks.soil == h.blocks.soil);
	UASSERT(r.species.size() == 1 && r.species[0].name == "oak");
	UASSERT(r.species[0].niche.heat[0] == h.species[0].niche.heat[0]);
	UASSERT(r.species[0].shape.rules_a == h.species[0].shape.rules_a);
	UASSERT(r.species[0].on == h.species[0].on);
}

void TestFar::testTileMatchesColumns()
{
	Horizon h = horizon();
	Generator gen(h, std::nullopt);
	Tile t = gen.build({ 0, 10, -5 });
	for (s32 j = -1; j <= TILE; j++)
		for (s32 i = -1; i <= TILE; i++) {
			Cell c = gen.column(10 * TILE + i, -5 * TILE + j);
			const Cell &got = t.at(i, j);
			UASSERT(got.ground == c.ground && got.top == c.top && got.water == c.water);
			// trees only stand on dry ground, above it
			if (!got.canopy.empty())
				UASSERT(got.canopy.low > got.ground && got.canopy.node.getContent() == 21 ||
						got.canopy.node.getContent() == 20);
		}
}

void TestFar::testCoarseCellsSampleCentres()
{
	Horizon h = horizon();
	Generator gen(h, std::nullopt);
	Tile t = gen.build({ 3, 1, 2 }); // cells 8 blocks wide
	const s32 cell = 8, x0 = 1 * TILE * cell, z0 = 2 * TILE * cell;
	for (s32 j = 0; j < TILE; j += 7)
		for (s32 i = 0; i < TILE; i += 5)
			UASSERT(t.at(i, j).ground == gen.column(x0 + i * cell + cell / 2, z0 + j * cell + cell / 2).ground);
}

void TestFar::testPolarWall()
{
	Horizon h = horizon();
	Generator gen(h, std::nullopt);
	Cell c = gen.column(0, gen.world().code().size / 2 + 10); // just past the pole
	UASSERTEQ(s32, c.ground, 1 + POLAR_WALL);
	UASSERT(c.top.getContent() == h.blocks.ice);
	UASSERT(!c.space);
	// past the wall, space: nothing to draw
	UASSERT(gen.column(0, gen.world().spaceFrom()).space);
}

// From high up the far view reaches the whole map, past both poles and
// into space: every tile builds.
void TestFar::testWholeMap()
{
	Horizon h = horizon();
	Generator gen(h, std::nullopt);
	const s32 level = 6, span = TILE << level, size = gen.world().code().size;
	for (s32 z = -size / span - 1; z <= size / span + 1; z++)
		for (s32 x = -size / span / 2; x < size / span / 2; x += 3)
			gen.build({ level, x, z });
}

// On a planet the map's columns are the world model's at their latitude and
// longitude: the poles are its poles, the faces' middles its meridians, and
// blocks either side of a face's edge are neighbours in the world model too
// (no cliff along a join). Planet 1024 is the default world's size round.
void TestFar::testOnThePlanet()
{
	const Horizon h = horizon();
	const planet::Planet p(1024);
	const terragen::World world(terragen::WorldCode::decode(h.code));
	const terra::Columns columns(world, h.blocks, h.water_level, &p);
	const int32_t size = world.code().size, half = size / 2;
	UASSERTEQ(int32_t, size, 8 * p.half());
	const auto middle = [&](int f) {
		const planet::Planet::Square s = p.square(f);
		return columns.onWorld(s.x + p.half(), s.z + p.half());
	};
	// (a face's middle is between nodes: the node there is half a node off it, a block of the world's)
	const auto near = [](int32_t a, int32_t b) { return std::abs(a - b) <= 1; };
	UASSERT(near(middle(4)[1], half) && near(middle(5)[1], 1 - half)); // the poles
	UASSERT(near(middle(0)[0], 0) && near(middle(0)[1], 0));           // longitude 0 on the equator
	UASSERT(near(middle(1)[0], size / 4) && near(middle(1)[1], 0));    // 90° east
	static const int DIRS[4][2] = { { 1, 0 }, { 0, 1 }, { -1, 0 }, { 0, -1 } };
	for (int f = 0; f < 6; f++)
	for (int d = 0; d < 4; d++) {
		const planet::Planet::Square s = p.square(f);
		for (int32_t i = 7; i < p.width(); i += 131) {
			const int32_t x = DIRS[d][0] > 0 ? s.x + p.width() - 1 : DIRS[d][0] < 0 ? s.x : s.x + i;
			const int32_t z = DIRS[d][1] > 0 ? s.z + p.width() - 1 : DIRS[d][1] < 0 ? s.z : s.z + i;
			const planet::Planet::Step out = p.step(x, z, DIRS[d][0], DIRS[d][1]);
			const auto a = columns.onWorld(x, z), b = columns.onWorld(out.x, out.z);
			if (std::abs(a[1]) > half * 8 / 9)
				continue; // near a pole longitude turns quickly; the polar caps take that
			const int32_t dx = std::abs(a[0] - b[0]), wrapped = std::min(dx, size - dx);
			UASSERT(wrapped <= 2 && std::abs(a[1] - b[1]) <= 2);
		}
	}
}
