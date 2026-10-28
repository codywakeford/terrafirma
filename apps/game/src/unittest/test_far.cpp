// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "test.h"

#include "far/horizon.h"
#include "far/globe.h"
#include "far/terrain.h"
#include "mapgen/terra_column.h"
#include "planet/atlas.h"
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
	void testGlobeFacesMeet();
	void testGlobeCellsAreBlocks();
	void testGlobeTileIsTheAtlasLand();
};

static TestFar g_test_instance;

void TestFar::runTests(IGameDef *gamedef)
{
	TEST(testPacketRoundTrip);
	TEST(testTileMatchesColumns);
	TEST(testCoarseCellsSampleCentres);
	TEST(testPolarWall);
	TEST(testWholeMap);
	TEST(testGlobeFacesMeet);
	TEST(testGlobeCellsAreBlocks);
	TEST(testGlobeTileIsTheAtlasLand);
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
	Generator gen(h);
	const s32 level = 6, span = TILE << level, size = gen.world().code().size;
	for (s32 z = -size / span - 1; z <= size / span + 1; z++)
		for (s32 x = -size / span / 2; x < size / span / 2; x += 3)
			gen.build({ level, x, z });
}

// An atlas planet as Terra Firma's worlds are: its far view tiles a globe.
static const terragen::Atlas::Params ATLAS = terragen::Atlas::standard(8192 / (2 * M_PI));

static double apart(const Globe::Vec &a, const Globe::Vec &b)
{
	return std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y) + (a.z - b.z) * (a.z - b.z));
}

// Every point along a face's edge is a point along another face's edge, the
// same distance along it: tiles on the two faces meet with no crack.
void TestFar::testGlobeFacesMeet()
{
	const Globe g(ATLAS.radius);
	const double n = g.cells();
	int edges = 0;
	for (int f = 0; f < 6; f++)
		for (int e = 0; e < 4; e++) {
			const auto at = [&](int face, int edge, double t) {
				const double fx = edge == 0 ? 0 : edge == 1 ? n : t, fz = edge == 2 ? 0 : edge == 3 ? n : t;
				return g.direction(face, fx, fz);
			};
			// the face across this edge: the one whose edge holds its middle
			const Globe::Vec mid = at(f, e, n / 2);
			int other = -1, oe = -1;
			bool flip = false;
			for (int f2 = 0; f2 < 6 && other < 0; f2++)
				for (int e2 = 0; e2 < 4 && other < 0; e2++)
					if (f2 != f && apart(at(f2, e2, n / 2), mid) < 1e-12) {
						other = f2;
						oe = e2;
						flip = apart(at(f2, e2, n / 4), at(f, e, n / 4)) > 1e-9;
					}
			UASSERT(other >= 0);
			for (double t : { 0.0, 1.0, 17.0, n / 3, n - 1, n })
				UASSERT(apart(at(f, e, t), at(other, oe, flip ? n - t : t)) < 1e-12);
			edges++;
		}
	UASSERTEQ(int, edges, 24);
}

// The finest cells are near squares about a block wide, at a face's middle
// and at its corners; a whole face is the top level's one tile.
void TestFar::testGlobeCellsAreBlocks()
{
	const Globe g(ATLAS.radius);
	UASSERT(g.cells() == (TILE << g.top()));
	for (double fx : { 0.5, g.cells() / 2.0, g.cells() - 0.5 })
		for (double fz : { 0.5, g.cells() / 2.0 }) {
			const auto p = [&](double dx, double dz) {
				const Globe::Vec d = g.direction(2, fx + dx, fz + dz);
				return Globe::Vec{ d.x * g.radius(), d.y * g.radius(), d.z * g.radius() };
			};
			const double across = apart(p(-0.5, 0), p(0.5, 0)), along = apart(p(0, -0.5), p(0, 0.5));
			UASSERT(across > 0.5 && across <= 1.0 && along > 0.5 && along <= 1.0);
			UASSERT(std::max(across, along) / std::min(across, along) < 1.5);
		}
}

// A globe tile's land is the atlas's: each cell the column the atlas's
// generator builds at its middle's latitude and longitude; and nearly every
// cell's ground is that of the atlas cell standing there (they sample the
// world model at nearby places).
void TestFar::testGlobeTileIsTheAtlasLand()
{
	Horizon h = horizon();
	const planet::Atlas atlas(ATLAS);
	Generator gen(h, atlas);
	const terra::Columns columns(gen.world(), h.blocks, h.water_level, &atlas);
	const Globe g(ATLAS.radius);
	int cells = 0, differ = 0;
	for (const TileKey key : { TileKey{ 0, 20, 31, 0 }, TileKey{ 0, 40, 2, 2 }, TileKey{ 2, 3, 5, 5 } }) {
		const Tile t = gen.build(key);
		for (s32 j = 0; j < TILE; j += 3)
			for (s32 i = 0; i < TILE; i += 3) {
				const s32 cell = key.cell();
				const s32 fx = key.x * key.span() + i * cell + cell / 2, fz = key.z * key.span() + j * cell + cell / 2;
				const Globe::Vec d = g.direction(key.face, fx, fz);
				const auto at = Globe::place(d);
				const terra::Column c = columns.atPlace(at[0], at[1]);
				UASSERTEQ(s32, t.at(i, j).ground, c.height);
				if (key.level > 0)
					continue;
				// the atlas cell at the ground there, and its own column
				const double r = ATLAS.radius + c.height + 0.5;
				const planet::Atlas::Node n = atlas.node(atlas.atlas().locate({ d.x * r, d.y * r, d.z * r }));
				const auto place = atlas.place(n);
				UASSERT(place.has_value());
				cells++;
				differ += columns.atPlace((*place)[0], (*place)[1]).height != c.height;
			}
	}
	UASSERT(cells > 100);
	UASSERT(differ * 10 < cells);
}
