// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2023 Vitaliy Lobachevskiy

#include "test.h"

#include "gamedef.h"
#include "inventory.h" // ItemStack
#include "dummygamedef.h"
#include "client/content_mapblock.h"
#include "client/mapblock_mesh.h"
#include "client/meshgen/collector.h"
#include "client/node_visuals.h"
#include <memory>
#include "mesh_compare.h"
#include "ground/ground.h" // Terra Firma
#include "noise.h"
#include <map>

namespace {

class MockGameDef : public DummyGameDef {
public:
	IWritableItemDefManager *item_mgr() noexcept {
		return static_cast<IWritableItemDefManager *>(m_itemdef);
	}

	NodeDefManager *node_mgr() noexcept {
		return const_cast<NodeDefManager *>(m_nodedef);
	}

	content_t registerNode(const ItemDefinition &itemdef, ContentFeatures &&nodedef) {
		item_mgr()->registerItem(itemdef);

		NodeDefManager *mgr = node_mgr();
		content_t id = mgr->set(nodedef.name, std::move(nodedef));

		return id;
	}

	void finalize() {
		node_mgr()->resolveCrossrefs();

		// Need to fill node visuals for predefined nodes
		node_mgr()->applyFunction([] (ContentFeatures &f) {
			if (!f.visuals)
				f.visuals = std::make_unique<NodeVisuals>();
		});
	}

	MeshMakeData makeSingleNodeMMD(bool smooth_lighting = true)
	{
		MeshMakeData data{ndef(), 1, MeshGrid{1}};
		data.m_generate_minimap = false;
		data.m_smooth_lighting = smooth_lighting;
		data.m_enable_water_reflections = false;
		data.m_blockpos = {0, 0, 0};
		// MapblockMeshGenerator needs a margin of at least 3
		for (s16 x = -3; x <= 3; x++)
		for (s16 y = -3; y <= 3; y++)
		for (s16 z = -3; z <= 3; z++)
			data.m_vmanip.setNode({x, y, z}, {CONTENT_AIR, 0, 0});
		return data;
	}

	content_t addSimpleNode(std::string name, u32 texture, bool loose = false)
	{
		ItemDefinition itemdef;
		itemdef.type = ITEM_NODE;
		itemdef.name = "test:" + name;
		itemdef.description = name;

		ContentFeatures f;
		f.visuals = std::make_unique<NodeVisuals>();
		f.name = itemdef.name;
		f.drawtype = NDT_NORMAL;
		f.alpha = ALPHAMODE_OPAQUE;
		for (TileDef &tiledef : f.tiledef)
			tiledef.name = name + ".png";
		for (TileSpec &tile : f.visuals->tiles)
			tile.layers[0].texture_id = texture;
		if (loose) // Terra Firma
			f.groups["loose"] = 1;

		return registerNode(itemdef, std::move(f));
	}

	content_t addLiquidSource(std::string name, u32 texture)
	{
		ItemDefinition itemdef;
		itemdef.type = ITEM_NODE;
		itemdef.name = "test:" + name + "_source";
		itemdef.description = name;

		ContentFeatures f;
		f.visuals = std::make_unique<NodeVisuals>();
		f.name = itemdef.name;
		f.drawtype = NDT_LIQUID;
		f.alpha = ALPHAMODE_BLEND;
		f.light_propagates = true;
		f.param_type = CPT_LIGHT;
		f.liquid_type = LIQUID_SOURCE;
		f.liquid_viscosity = 4;
		f.groups["liquids"] = 3;
		f.liquid_alternative_source = "test:" + name + "_source";
		f.liquid_alternative_flowing = "test:" + name + "_flowing";
		for (TileDef &tiledef : f.tiledef)
			tiledef.name = name + ".png";
		for (TileSpec &tile : f.visuals->tiles)
			tile.layers[0].texture_id = texture;

		return registerNode(itemdef, std::move(f));
	}

	content_t addLiquidFlowing(std::string name, u32 texture_top, u32 texture_side)
	{
		ItemDefinition itemdef;
		itemdef.type = ITEM_NODE;
		itemdef.name = "test:" + name + "_flowing";
		itemdef.description = name;

		ContentFeatures f;
		f.visuals = std::make_unique<NodeVisuals>();
		f.name = itemdef.name;
		f.drawtype = NDT_FLOWINGLIQUID;
		f.alpha = ALPHAMODE_BLEND;
		f.light_propagates = true;
		f.param_type = CPT_LIGHT;
		f.liquid_type = LIQUID_FLOWING;
		f.liquid_viscosity = 4;
		f.groups["liquids"] = 3;
		f.liquid_alternative_source = "test:" + name + "_source";
		f.liquid_alternative_flowing = "test:" + name + "_flowing";
		f.tiledef_special[0].name = name + "_top.png";
		f.tiledef_special[1].name = name + "_side.png";
		f.visuals->special_tiles[0].layers[0].texture_id = texture_top;
		f.visuals->special_tiles[1].layers[0].texture_id = texture_side;

		return registerNode(itemdef, std::move(f));
	}
};

void set_light_decode_table()
{
	u8 table[LIGHT_SUN + 1] = {
		0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
		0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF,
	};
	memcpy(const_cast<u8 *>(light_decode_table), table, sizeof(table));
}

class TestMapblockMeshGenerator : public TestBase {
public:
	TestMapblockMeshGenerator() { TestManager::registerTestModule(this); }
	const char *getName() override { return "TestMapblockMeshGenerator"; }

	void runTests(IGameDef *gamedef) override;
	void testSimpleNode();
	void testSurroundedNode();
	void testInterliquidSame();
	void testInterliquidDifferent();
	void testSmoothedGroundHasNoHoles(); // Terra Firma
};

static TestMapblockMeshGenerator g_test_instance;

void TestMapblockMeshGenerator::runTests(IGameDef *gamedef)
{
	set_light_decode_table();
	TEST(testSimpleNode);
	TEST(testSurroundedNode);
	TEST(testInterliquidSame);
	TEST(testInterliquidDifferent);
	TEST(testSmoothedGroundHasNoHoles);
}

namespace quad {
	constexpr float h = BS / 2.0f;
	const Quad zp{{{{-h, -h, h}, {0, 0, 1}, 0, {1, 1}}, {{h, -h, h}, {0, 0, 1}, 0, {0, 1}}, {{h, h, h}, {0, 0, 1}, 0, {0, 0}}, {{-h, h, h}, {0, 0, 1}, 0, {1, 0}}}};
	const Quad yp{{{{-h, h, -h}, {0, 1, 0}, 0, {0, 1}}, {{-h, h, h}, {0, 1, 0}, 0, {0, 0}}, {{h, h, h}, {0, 1, 0}, 0, {1, 0}}, {{h, h, -h}, {0, 1, 0}, 0, {1, 1}}}};
	const Quad xp{{{{h, -h, -h}, {1, 0, 0}, 0, {0, 1}}, {{h, h, -h}, {1, 0, 0}, 0, {0, 0}}, {{h, h, h}, {1, 0, 0}, 0, {1, 0}}, {{h, -h, h}, {1, 0, 0}, 0, {1, 1}}}};
	const Quad zn{{{{-h, -h, -h}, {0, 0, -1}, 0, {0, 1}}, {{-h, h, -h}, {0, 0, -1}, 0, {0, 0}}, {{h, h, -h}, {0, 0, -1}, 0, {1, 0}}, {{h, -h, -h}, {0, 0, -1}, 0, {1, 1}}}};
	const Quad yn{{{{-h, -h, -h}, {0, -1, 0}, 0, {0, 0}}, {{h, -h, -h}, {0, -1, 0}, 0, {1, 0}}, {{h, -h, h}, {0, -1, 0}, 0, {1, 1}}, {{-h, -h, h}, {0, -1, 0}, 0, {0, 1}}}};
	const Quad xn{{{{-h, -h, -h}, {-1, 0, 0}, 0, {1, 1}}, {{-h, -h, h}, {-1, 0, 0}, 0, {0, 1}}, {{-h, h, h}, {-1, 0, 0}, 0, {0, 0}}, {{-h, h, -h}, {-1, 0, 0}, 0, {1, 0}}}};
}

void TestMapblockMeshGenerator::testSimpleNode()
{
	MockGameDef gamedef;
	content_t stone = gamedef.addSimpleNode("stone", 42);
	gamedef.finalize();

	MeshMakeData data = gamedef.makeSingleNodeMMD();
	data.m_vmanip.setNode({0, 0, 0}, {stone, 0, 0});

	MeshCollector col{{}};
	MapblockMeshGenerator mg{&data, &col};
	mg.generate();
	UASSERTEQ(std::size_t, col.prebuffers[0].size(), 1);
	UASSERTEQ(std::size_t, col.prebuffers[1].size(), 0);

	auto &&buf = col.prebuffers[0][0];
	UASSERTEQ(u32, buf.layer.texture_id, 42);
	UASSERT(checkMeshEqual(buf.vertices, buf.indices, {quad::xn, quad::xp, quad::yn, quad::yp, quad::zn, quad::zp}));
}

void TestMapblockMeshGenerator::testSurroundedNode()
{
	MockGameDef gamedef;
	content_t stone = gamedef.addSimpleNode("stone", 42);
	content_t wood = gamedef.addSimpleNode("wood", 13);
	gamedef.finalize();

	MeshMakeData data = gamedef.makeSingleNodeMMD();
	data.m_vmanip.setNode({0, 0, 0}, {stone, 0, 0});
	data.m_vmanip.setNode({1, 0, 0}, {wood, 0, 0});

	MeshCollector col{{}};
	MapblockMeshGenerator mg{&data, &col};
	mg.generate();
	UASSERTEQ(std::size_t, col.prebuffers[0].size(), 1);
	UASSERTEQ(std::size_t, col.prebuffers[1].size(), 0);

	auto &&buf = col.prebuffers[0][0];
	UASSERTEQ(u32, buf.layer.texture_id, 42);
	UASSERT(checkMeshEqual(buf.vertices, buf.indices, {quad::xn, quad::yn, quad::yp, quad::zn, quad::zp}));
}

void TestMapblockMeshGenerator::testInterliquidSame()
{
	MockGameDef gamedef;
	auto water = gamedef.addLiquidSource("water", 42);
	gamedef.finalize();

	MeshMakeData data = gamedef.makeSingleNodeMMD();
	data.m_vmanip.setNode({0, 0, 0}, {water, 0, 0});
	data.m_vmanip.setNode({1, 0, 0}, {water, 0, 0});

	MeshCollector col{{}};
	MapblockMeshGenerator mg{&data, &col};
	mg.generate();
	UASSERTEQ(std::size_t, col.prebuffers[0].size(), 1);
	UASSERTEQ(std::size_t, col.prebuffers[1].size(), 0);

	auto &&buf = col.prebuffers[0][0];
	UASSERTEQ(u32, buf.layer.texture_id, 42);
	UASSERT(checkMeshEqual(buf.vertices, buf.indices, {quad::xn, quad::yn, quad::yp, quad::zn, quad::zp}));
}

void TestMapblockMeshGenerator::testInterliquidDifferent()
{
	MockGameDef gamedef;
	auto water = gamedef.addLiquidSource("water", 42);
	auto lava = gamedef.addLiquidSource("lava", 13);
	gamedef.finalize();

	MeshMakeData data = gamedef.makeSingleNodeMMD();
	data.m_vmanip.setNode({0, 0, 0}, {water, 0, 0});
	data.m_vmanip.setNode({0, 0, 1}, {lava, 0, 0});

	MeshCollector col{{}};
	MapblockMeshGenerator mg{&data, &col};
	mg.generate();
	UASSERTEQ(std::size_t, col.prebuffers[0].size(), 1);
	UASSERTEQ(std::size_t, col.prebuffers[1].size(), 0);

	auto &&buf = col.prebuffers[0][0];
	UASSERTEQ(u32, buf.layer.texture_id, 42);
	UASSERT(checkMeshEqual(buf.vertices, buf.indices, {quad::xn, quad::xp, quad::yn, quad::yp, quad::zn, quad::zp}));
}


/*
	Terra Firma: a block of rough ground, dirt (loose) and some stone, meshed.
	Straight down, a ray meets the ground's surface the rule gives (what
	collision will stand on); slanting, it reaches the mesh through no ground
	(a crack would let it through to a surface beyond). With the walls not
	drawn, the slanting rays fail.
*/
void TestMapblockMeshGenerator::testSmoothedGroundHasNoHoles()
{
	MockGameDef gamedef;
	const content_t stone = gamedef.addSimpleNode("stone", 42);
	const content_t dirt = gamedef.addSimpleNode("dirt", 13, true);
	gamedef.finalize();

	const s16 N = 16, M = 6; // the block, and a margin round it
	std::map<std::pair<int, int>, std::pair<int, bool>> columns; // top, solid
	for (int x = -M; x < N + M; x++)
		for (int z = -M; z < N + M; z++) {
			float n = noise2d_value(x / 4.f, z / 4.f, 7) + 0.6f * noise2d(x, z, 11);
			columns[{ x, z }] = { 8 + int(std::floor(n * 2)), noise2d(x * 5, z * 5, 13) > 0.75f };
		}

	MeshMakeData data{gamedef.ndef(), N, MeshGrid{1}};
	data.m_generate_minimap = false;
	data.m_smooth_lighting = true;
	data.m_enable_water_reflections = false;
	data.m_blockpos = {0, 0, 0};
	for (s16 x = -M; x < N + M; x++)
	for (s16 y = -M; y < N + M; y++)
	for (s16 z = -M; z < N + M; z++) {
		auto [top, solid] = columns[{ x, z }];
		data.m_vmanip.setNode({x, y, z}, {content_t(y < top ? (solid ? stone : dirt) : CONTENT_AIR), 0, 0});
	}

	MeshCollector col{{}};
	MapblockMeshGenerator mg{&data, &col};
	mg.generate();
	std::vector<std::array<v3f, 3>> triangles;
	for (auto &layer : col.prebuffers)
		for (auto &buf : layer)
			for (size_t i = 0; i + 2 < buf.indices.size(); i += 3)
				triangles.push_back({ buf.vertices[buf.indices[i]].Pos / BS,
						buf.vertices[buf.indices[i + 1]].Pos / BS, buf.vertices[buf.indices[i + 2]].Pos / BS });

	// the ground's surface at (x, z), in node units (a node's centre at its
	// position): a smoothed column's field, a cube's top
	const ground::Ground ground(*gamedef.ndef());
	auto get = [&](v3s16 p) { return data.m_vmanip.getNodeNoExNoEmerge(p); };
	auto surface = [&](float x, float z) {
		int cx = int(std::floor(x + 0.5f)), cz = int(std::floor(z + 0.5f));
		int top = columns[{ cx, cz }].first;
		auto t = ground.top(get, v3s16(cx, top - 1, cz));
		float h = t ? t->height(x + 0.5f - cx, z + 0.5f - cz) : 1.f;
		return top - 1.5f + h;
	};
	// the first hit along a ray, if any (both faces)
	auto hit = [&](v3f o, v3f d) -> std::optional<v3f> {
		std::optional<float> best;
		for (const auto &t : triangles) {
			v3f e1 = t[1] - t[0], e2 = t[2] - t[0], pv = d.crossProduct(e2);
			float det = e1.dotProduct(pv);
			if (std::fabs(det) < 1e-9f)
				continue;
			v3f tv = o - t[0];
			float u = tv.dotProduct(pv) / det;
			if (u < -1e-5f || u > 1 + 1e-5f)
				continue;
			v3f qv = tv.crossProduct(e1);
			float v = d.dotProduct(qv) / det;
			if (v < -1e-5f || u + v > 1 + 1e-5f)
				continue;
			float k = e2.dotProduct(qv) / det;
			if (k > 0 && (!best || k < *best))
				best = k;
		}
		if (!best)
			return std::nullopt;
		return o + d * *best;
	};

	PseudoRandom random(5);
	auto in = [&](float margin) { return -0.5f + margin + random.next() / 32767.f * (N - 2 * margin); };
	int shaped = 0;
	for (int r = 0; r < 3000; r++) {
		float x = in(0.01f), z = in(0.01f);
		// off the edges between columns, where a wall stands
		auto off = [](float v) { float f = v + 0.5f - std::floor(v + 0.5f); return f > 1e-3f && f < 1 - 1e-3f; };
		if (!off(x) || !off(z))
			continue;
		auto p = hit(v3f(x, 40, z), v3f(0, -1, 0));
		UASSERT(p);
		UASSERT(std::fabs(p->Y - surface(x, z)) < 1e-3f);
		shaped += std::fabs(p->Y - std::round(p->Y + 0.5f) + 0.5f) > 1e-3f;
	}
	for (int r = 0; r < 3000; r++) {
		// slanting 30° from straight down in a random direction, from above
		// all the ground (tops 4..11: it stands at most 11.5) to where it
		// meets it at (x, z), if not before: at most 4.6 across, so all of its
		// way down is over the block (outside it nothing's drawn)
		float a = random.next() / 32767.f * 6.2832f;
		v3f d(std::cos(a) * 0.5f, -0.866f, std::sin(a) * 0.5f);
		float x = in(5.f), z = in(5.f);
		v3f aim(x, surface(x, z), z);
		v3f o = aim - d * ((12.f - aim.Y) / 0.866f);
		auto p = hit(o, d);
		UASSERT(p);
		// all its way there in the open: through no ground (a crack would let
		// it pass through ground to a surface beyond)
		const float length = (*p - o).getLength();
		for (float k = 0; k < length - 1e-2f; k += 0.01f) {
			v3f q = o + d * k;
			float fx = q.X + 0.5f - std::floor(q.X + 0.5f), fz = q.Z + 0.5f - std::floor(q.Z + 0.5f);
			if (fx < 1e-3f || fx > 1 - 1e-3f || fz < 1e-3f || fz > 1 - 1e-3f)
				continue; // on an edge: either side's
			UASSERT(q.Y > surface(q.X, q.Z) - 1e-3f);
		}
	}
	UASSERT(shaped > 500); // enough of it was sloped to mean something
}

}
