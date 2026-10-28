// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "test.h"

#include <set>

#include "far/horizon.h"
#include "far/tiling.h"
#include "mapgen/atlas_land.h"
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
	void testTilingIsTheAtlas();
	void testTileIsTheAtlasLand();
};

static TestFar g_test_instance;

void TestFar::runTests(IGameDef *gamedef)
{
	TEST(testPacketRoundTrip);
	TEST(testTileMatchesColumns);
	TEST(testCoarseCellsSampleCentres);
	TEST(testPolarWall);
	TEST(testWholeMap);
	TEST(testTilingIsTheAtlas);
	TEST(testTileIsTheAtlasLand);
}

// A horizon with made-up block ids: the far view only compares them.
static Horizon horizon()
{
	Horizon h;
	h.code = "TF5.dGVycmE";
	h.water_level = 1;
	terra::Blocks &b = h.content.blocks;
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
	h.content.species.push_back(oak);
	return h;
}

void TestFar::testPacketRoundTrip()
{
	Horizon h = horizon();
	h.content.species[0].niche.heat[0] = -0.123456789012345; // exact bits, not a rounding
	NetworkPacket out(TOCLIENT_HORIZON, 0);
	write(out, h);
	// as it travels: the command, then the body
	Buffer<u8> wire = out.oldForgePacket();
	NetworkPacket in;
	in.putRawPacket(*wire, wire.getSize(), 0);
	Horizon r = read(in);
	UASSERT(r.code == h.code && r.water_level == h.water_level);
	UASSERT(r.content.blocks.biomes[3].filler == h.content.blocks.biomes[3].filler);
	UASSERT(r.content.blocks.rock[2] == h.content.blocks.rock[2] && r.content.blocks.soil == h.content.blocks.soil);
	UASSERT(r.content.species.size() == 1 && r.content.species[0].name == "oak");
	UASSERT(r.content.species[0].niche.heat[0] == h.content.species[0].niche.heat[0]);
	UASSERT(r.content.species[0].shape.rules_a == h.content.species[0].shape.rules_a);
	UASSERT(r.content.species[0].on == h.content.species[0].on);
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
	UASSERT(c.top.getContent() == h.content.blocks.ice);
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

// An atlas planet as Terra Firma's worlds are: its far view tiles its regions.
static const terragen::Atlas::Params ATLAS = terragen::Atlas::standard(8192 / (2 * M_PI));

static double apart(const Tiling::Vec &a, const Tiling::Vec &b)
{
	return std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y) + (a.z - b.z) * (a.z - b.z));
}

// The tiling's roots are the regions with a surface, each covering its whole
// grid; a tile's cell is the atlas cell it stands for, and its point there is
// that cell's centre (the region's own chart).
void TestFar::testTilingIsTheAtlas()
{
	const terragen::Atlas atlas(ATLAS);
	const Tiling t(atlas);
	// every region with a surface, as squares covering its grid; each root's
	// middle is in its region (on its grid's own part of the planet)
	std::set<s32> regions;
	for (const TileKey &root : t.roots()) {
		const terragen::Atlas::Grid &g = atlas.grids().at(root.face);
		UASSERT(g.region != terragen::Atlas::Region::Core);
		UASSERT(t.within(root));
		regions.insert(root.face);
		const Tiling::Vec m = t.middle(root);
		const terragen::Atlas::Cell owner = atlas.owner({ m.x, m.y + 0, m.z });
		UTEST(owner.region == g.region && owner.index == g.index, "root (%d, %d) of grid %d has its middle in region %d/%d",
				root.x, root.z, root.face, (int)owner.region, owner.index);
	}
	UASSERTEQ(size_t, regions.size(), (size_t)ATLAS.segments + 2); // the segments and the caps
	for (const TileKey key : { TileKey{ 0, 3, 40, t.roots()[0].face }, TileKey{ 0, 1, 2, t.roots().back().face } })
		for (s32 j = 0; j < TILE; j += 7)
			for (s32 i = 0; i < TILE; i += 5) {
				const terragen::Atlas::Cell c = t.cell(key, i, j);
				UASSERT(apart(t.point(key, i, 0, j), atlas.centre(c)) < 1e-6);
			}
}

// A level-0 tile is the game's surface, cell for cell: where the atlas has a
// cell, its ground is the column terra::AtlasLand builds there and its top
// block AtlasLand's at that layer; where it has none, nothing is drawn.
void TestFar::testTileIsTheAtlasLand()
{
	Horizon h = horizon();
	const planet::Atlas atlas(ATLAS);
	Generator gen(h, atlas);
	terra::AtlasLand::Reader land(gen.land());
	const Tiling t(atlas.atlas());
	int cells = 0, gaps = 0;
	// a segment's middle, its east edge (cut cells, the gap past the grid), a cap
	const TileKey seg = t.roots()[0], cap = t.roots().back();
	const terragen::Atlas::Grid &g = atlas.atlas().grids().at(seg.face);
	const s32 east = (s32)((g.high[0] - g.low[0]) / TILE);
	for (const TileKey key : { TileKey{ 0, 2, 40, seg.face }, TileKey{ 0, east, 40, seg.face }, TileKey{ 0, 3, 3, cap.face } }) {
		const Tile tile = gen.build(key);
		for (s32 j = 0; j < TILE; j++)
			for (s32 i = 0; i < TILE; i++) {
				const Cell &c = tile.at(i, j);
				const terragen::Atlas::Cell cell = t.cell(key, i, j);
				UASSERT(c.space == !atlas.atlas().exists(cell));
				if (c.space) {
					gaps++;
					continue;
				}
				cells++;
				const planet::Atlas::Node n = atlas.node(cell);
				const terra::Column *column = land.column(n);
				UASSERT(column);
				UASSERTEQ(s32, c.ground, column->height);
				const std::optional<MapNode> top = land.node({ n.x, c.ground, n.z });
				if (top) // the ground's own layer can be trimmed where the surface one isn't
					UASSERT(top->getContent() == c.top.getContent());
			}
	}
	UASSERT(cells > 2000 && gaps > 0);
}
