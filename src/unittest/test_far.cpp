// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "test.h"

#include "far/horizon.h"
#include "far/terrain.h"
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
};

static TestFar g_test_instance;

void TestFar::runTests(IGameDef *gamedef)
{
	TEST(testPacketRoundTrip);
	TEST(testTileMatchesColumns);
	TEST(testCoarseCellsSampleCentres);
	TEST(testPolarWall);
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
	Generator gen(h);
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
	Generator gen(h);
	Tile t = gen.build({ 3, 1, 2 }); // cells 8 blocks wide
	const s32 cell = 8, x0 = 1 * TILE * cell, z0 = 2 * TILE * cell;
	for (s32 j = 0; j < TILE; j += 7)
		for (s32 i = 0; i < TILE; i += 5)
			UASSERT(t.at(i, j).ground == gen.column(x0 + i * cell + cell / 2, z0 + j * cell + cell / 2).ground);
}

void TestFar::testPolarWall()
{
	Horizon h = horizon();
	Generator gen(h);
	Cell c = gen.column(0, gen.world().code().size); // well beyond the north–south extent
	UASSERTEQ(s32, c.ground, 1 + POLAR_WALL);
	UASSERT(c.top.getContent() == h.blocks.ice);
}
