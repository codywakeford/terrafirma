// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "test.h"

#include "far/baked.h"
#include "far/mesh.h"
#include "far/tiles.h"
#include "exceptions.h"
#include "filesys.h"
#include "light.h"
#include "porting.h"
#include <chrono>
#include <cmath>
#include <fstream>
#include <thread>

using namespace far;

/*
	The baked palette (far/baked.h) and the tiles drawn with it (far/tiles.h),
	with no game running: what the menu and the Planetarium have.
*/
class TestFarPalette : public TestBase {
public:
	TestFarPalette() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestFarPalette"; }

	void runTests(IGameDef *gamedef);

	void testBakedRoundTrip();
	void testShippedPaletteIsWhole();
	void testTilesForAnEye();
	void testSeaIsWater();
};

static TestFarPalette g_test_instance;

void TestFarPalette::runTests(IGameDef *gamedef)
{
	set_light_curve(1.0f); // tiles are lit by it, as the client sets it
	TEST(testBakedRoundTrip);
	TEST(testShippedPaletteIsWhole);
	TEST(testTilesForAnEye);
	TEST(testSeaIsWater);
}

namespace {

// Content with made-up ids, every block a look of its own colour; the soil
// tinted by a palette.
std::pair<Content, Palette> made()
{
	Content c;
	terra::Blocks &b = c.blocks;
	content_t next = 10;
	for (terra::BiomeBlocks &bb : b.biomes)
		bb = { next++, next++, next++, 11, 1, 3, 1, 0 };
	b.stone = next++;
	b.water = 11;
	b.ice = next++;
	b.lava = next++;
	b.placer = next++;
	b.soil = next++;
	for (content_t &r : b.rock)
		r = next++;
	b.ore[0] = CONTENT_IGNORE;
	for (size_t i = 1; i < std::size(b.ore); i++)
		b.ore[i] = next++;
	terra::Species oak{ "oak", { { -0.123456789012345, 1 }, { -1, 1 }, true, { 0.25, 0.5 }, true, false }, {}, next, next + 1,
		{ "FFFFA", "[&&fff]", "", "", "", "single", 45, 1, 0, true } };
	next += 2;
	oak.on.push_back(b.biomes[0].top);
	c.species.push_back(oak);

	Palette p;
	for (content_t id : blocksIn(c)) {
		if (id == CONTENT_IGNORE || p.looks().count(id))
			continue;
		Palette::Look l{};
		for (int k = 0; k < 6; k++) {
			l.face[k] = video::SColorf(id / 256.0f, k / 6.0f, 0.1f + 1.0f / 3.0f, 1);
			l.tinted[k] = k % 2;
		}
		l.tints.assign(id == b.soil ? 256 : 1, video::SColorf(0.7f, 0.2f, 0.123456f, 1));
		if (id == b.soil)
			l.tints[200] = video::SColorf(0.1f, 0.9f, 0.3f, 1);
		l.over_black = video::SColorf(0.01f, 0.02f, 0.03f, 1);
		l.clarity = id == b.water ? 0.4f : 0;
		p.add(id, std::move(l));
	}
	return { c, p };
}

std::string nameOf(content_t id)
{
	return "test:b" + std::to_string(id);
}

// The game's own, as staged next to the binary.
Baked shipped()
{
	return Baked::load("terrafirma");
}

} // namespace

// Written and read back, every role is the same block (by name) and every
// look the same, bit for bit.
void TestFarPalette::testBakedRoundTrip()
{
	const auto [content, palette] = made();
	Json::StreamWriterBuilder writer;
	writer["precision"] = 17;
	Json::Value json;
	std::istringstream text(Json::writeString(writer, Baked::write(content, palette, nameOf)));
	text >> json;
	const Baked b = Baked::read(json);
	const auto same = [&](content_t was, content_t now) {
		return was == CONTENT_IGNORE ? now == CONTENT_IGNORE : b.names.at(now) == nameOf(was);
	};
	const terra::Blocks &x = content.blocks, &y = b.content.blocks;
	UASSERT(same(x.stone, y.stone) && same(x.water, y.water) && same(x.soil, y.soil) && same(x.placer, y.placer));
	for (size_t i = 0; i < std::size(x.biomes); i++)
		UASSERT(same(x.biomes[i].top, y.biomes[i].top) && same(x.biomes[i].filler, y.biomes[i].filler) &&
				x.biomes[i].depth_filler == y.biomes[i].depth_filler);
	for (size_t i = 0; i < std::size(x.ore); i++)
		UASSERT(same(x.ore[i], y.ore[i]));
	const terra::Species &s = b.content.species.at(0);
	UASSERT(s.niche.heat[0] == -0.123456789012345 && s.niche.season[1] == 0.5 && s.niche.seasonal && s.niche.bank);
	UASSERT(same(content.species[0].trunk, s.trunk) && same(content.species[0].on[0], s.on.at(0)));
	UASSERT(s.shape.rules_a == "[&&fff]" && s.shape.angle == 45 && s.shape.thin_branches);
	for (const auto &[id, l] : palette.looks()) {
		content_t found = CONTENT_IGNORE;
		for (content_t i = 0; i < b.names.size(); i++)
			if (b.names[i] == nameOf(id))
				found = i;
		const Palette::Look &r = b.palette.look(found);
		for (int k = 0; k < 6; k++)
			UASSERT(r.face[k].r == l.face[k].r && r.face[k].g == l.face[k].g && r.face[k].b == l.face[k].b &&
					r.tinted[k] == l.tinted[k]);
		UASSERT(r.tints.size() == l.tints.size() && r.clarity == l.clarity && r.over_black.g == l.over_black.g);
		for (size_t i = 0; i < l.tints.size(); i++)
			UASSERT(r.tints[i].r == l.tints[i].r && r.tints[i].b == l.tints[i].b);
	}
	// a face takes its param2's tint
	const MapNode soil(b.content.blocks.soil, 0, 200);
	UASSERT(std::fabs(b.palette.face(soil, EAST).g - palette.face(MapNode(content.blocks.soil, 0, 200), EAST).g) == 0);
}

// The game's palette knows every block its content places.
void TestFarPalette::testShippedPaletteIsWhole()
{
	const Baked b = shipped();
	UASSERT(!b.content.species.empty());
	for (content_t id : blocksIn(b.content))
		if (id != CONTENT_IGNORE && id != CONTENT_AIR)
			b.palette.look(id); // throws if missing
}

// For an eye over a small planet, built from the game's palette with no
// game: tiles come, nothing past the eye's reach is drawn, and tiles near
// the eye are finer than those far off.
void TestFarPalette::testTilesForAnEye()
{
	const Baked b = shipped();
	const Horizon h{ "TF5.dGVycmE", 1, b.content };
	const planet::Atlas atlas({ 300, 12, 80, 60, 0.95, 32 });
	Tiles tiles(atlas, h, b.palette, 2);
	const double sea = 300 + h.water_level + 1;
	const Eye eye{ { sea, 0, 0 }, 40, 0, 500, 2 };
	std::vector<Tiles::Drawn> drawn;
	for (int i = 0; i < 600; i++) {
		tiles.update(eye);
		drawn = tiles.drawn();
		if (!drawn.empty() && i > 100)
			break;
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
	UASSERT(!drawn.empty());
	const Tiling &g = tiles.tiling();
	s32 nearest = 99, farthest = -1;
	double near_d = 1e30, far_d = -1;
	for (const Tiles::Drawn &t : drawn) {
		const double d = std::hypot(t.middle.x * sea / g.radius() - eye.at.x, t.middle.y * sea / g.radius() - eye.at.y,
				t.middle.z * sea / g.radius() - eye.at.z) - g.diagonal(t.key) / 2;
		UTEST(d <= eye.range, "a tile %.0f blocks off is drawn past the eye's %.0f", d, eye.range);
		if (d < near_d) { near_d = d; nearest = t.key.level; }
		if (d > far_d) { far_d = d; farthest = t.key.level; }
	}
	UTEST(nearest < farthest, "the nearest tile (level %d) isn't finer than the farthest (%d)", nearest, farthest);
}


// A tile of sea, from the game's palette: its water's top is the water's
// colour seen over the floor, not black (it once was: lit by an unset light
// curve, 0 / 0).
void TestFarPalette::testSeaIsWater()
{
	const Baked b = shipped();
	Tile t;
	t.key = { 0, 0, 0, 0 };
	t.cells.resize((TILE + 2) * (TILE + 2));
	const MapNode floor(b.content.blocks.biomes[0].top), water(b.content.blocks.water);
	for (Cell &c : t.cells) {
		c = Cell{};
		c.ground = c.top_end = c.filler_end = -10;
		c.top = c.filler = c.rock = floor;
		c.water = 1;
		c.liquid = water;
	}
	const Mesh m = far::mesh(t, b.palette);
	// at least the water's own colour over black (light only adds the floor's)
	const video::SColor least = b.palette.look(water.getContent()).over_black.toSColor();
	size_t tops = 0;
	for (const Mesh::Part &p : m.parts)
		for (const video::S3DVertex &v : p.vertices)
			if (v.Color.getAlpha() == 0) { // a liquid's top (far/mesh.cpp)
				tops++;
				UASSERT(v.Color.getRed() + v.Color.getGreen() + v.Color.getBlue() > 60);
				UASSERT(v.Color.getGreen() + 1 >= least.getGreen() && v.Color.getBlue() + 1 >= least.getBlue());
			}
	UASSERT(tops > 0);
}


