// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "mesh.h"
#include "client/node_visuals.h"
#include "client/texturesource.h"
#include "constants.h"
#include "exceptions.h"
#include "light.h"
#include "nodedef.h"
#include <algorithm>
#include <cmath>

namespace far {

// The near mesh's shade per face (client/mesh.cpp applyFacesShading).
static const float SHADE[6] = { 1.0f, 0.447213f, 0.670820f, 0.670820f, 0.836660f, 0.836660f };
static const v3f NORMAL[6] = { { 0, 1, 0 }, { 0, -1, 0 }, { 1, 0, 0 }, { -1, 0, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };

static video::SColorf toColorf(video::SColor c)
{
	return video::SColorf(c.getRed() / 255.0f, c.getGreen() / 255.0f, c.getBlue() / 255.0f, 1.0f);
}

Colours::Colours(const NodeDefManager *ndef, ITextureSource *tsrc, const std::vector<content_t> &blocks) :
	m_ndef(ndef)
{
	for (content_t id : blocks) {
		if (id == CONTENT_IGNORE || id == CONTENT_AIR || m_entries.count(id))
			continue;
		const ContentFeatures &f = ndef->get(id);
		Entry e{};
		for (int k = 0; k < 6; k++) {
			const TileDef &t = f.tiledef[k];
			if (t.name.empty())
				throw BaseException("far view: block '" + f.name + "' has a face with no texture");
			const std::string &overlay = f.tiledef_overlay[k].name;
			std::string name = overlay.empty() ? t.name : t.name + "^(" + overlay + ")";
			e.face[k] = toColorf(tsrc->getTextureAverageColor(name));
			e.tile_colour[k] = t.has_color;
			if (t.has_color) {
				video::SColorf c = toColorf(t.color);
				e.face[k] = video::SColorf(e.face[k].r * c.r, e.face[k].g * c.g, e.face[k].b * c.b, 1);
			}
		}
		// the average leaves alpha out: over black and over white, what shows through is the difference
		const std::string &top = f.tiledef[0].name;
		video::SColorf on_black = toColorf(tsrc->getTextureAverageColor("[fill:1x1:#000000^(" + top + ")"));
		video::SColorf on_white = toColorf(tsrc->getTextureAverageColor("[fill:1x1:#ffffff^(" + top + ")"));
		e.clarity = std::clamp(((on_white.r - on_black.r) + (on_white.g - on_black.g) + (on_white.b - on_black.b)) / 3.0f,
				0.0f, 1.0f);
		e.over_black = on_black;
		m_entries.emplace(id, e);
	}
}

video::SColorf Colours::face(MapNode n, Face f) const
{
	auto it = m_entries.find(n.getContent());
	if (it == m_entries.end())
		throw BaseException("far view: no colour for block " + std::to_string(n.getContent()));
	const Entry &e = it->second;
	video::SColorf c = e.face[f];
	if (!e.tile_colour[f]) {
		const ContentFeatures &cf = m_ndef->get(n);
		video::SColorf tint = toColorf(cf.visuals->getColor(cf, n.param2));
		c = video::SColorf(c.r * tint.r, c.g * tint.g, c.b * tint.b, 1);
	}
	return c;
}

video::SColorf Colours::through(MapNode liquid, video::SColorf floor, float lit) const
{
	auto it = m_entries.find(liquid.getContent());
	if (it == m_entries.end())
		throw BaseException("far view: no colour for block " + std::to_string(liquid.getContent()));
	const Entry &e = it->second;
	// colour × opacity, plus what shows through
	return video::SColorf(e.over_black.r + floor.r * lit * e.clarity, e.over_black.g + floor.g * lit * e.clarity,
			e.over_black.b + floor.b * lit * e.clarity, 1);
}

std::vector<content_t> blocksIn(const Horizon &h)
{
	const terra::Blocks &b = h.blocks;
	std::vector<content_t> out = { b.stone, b.water, b.ice, b.lava, b.placer, b.soil };
	for (const terra::BiomeBlocks &bb : b.biomes)
		out.insert(out.end(), { bb.top, bb.filler, bb.riverbed, bb.water_top });
	out.insert(out.end(), std::begin(b.rock), std::end(b.rock));
	out.insert(out.end(), std::begin(b.ore), std::end(b.ore));
	for (const terra::Species &s : h.species)
		out.insert(out.end(), { s.trunk, s.leaves });
	return out;
}

namespace {

// What a face is, carried in its vertex alpha for the shader: where the near
// world draws a column, our solid faces give way to its; where it draws its
// own liquid, our liquid's do; what lies under a liquid never gives way (the
// near world's may not be there yet, and its liquid would lie over nothing).
enum class Layer { SOLID, LIQUID, UNDER };
static float alphaOf(Layer l)
{
	return l == Layer::SOLID ? 1.0f : l == Layer::LIQUID ? 0.0f : 0.5f;
}

// Collects quads into parts of at most 65536 vertices.
class Builder
{
public:
	Mesh mesh;

	// a quad from four corners (in blocks), wound to face `face`. A liquid's
	// faces carry alpha 0: the shader drops them where the near world draws
	// its own liquid there.
	void quad(const v3f (&corner)[4], Face face, video::SColorf colour, Layer layer = Layer::SOLID)
	{
		quad(corner, face, { 1, 1, 1, 1 }, colour, layer);
	}

	// with each corner lit to its own level (0..1), as smooth lighting does
	void quad(const v3f (&corner)[4], Face face, const float (&lit)[4], video::SColorf colour, Layer layer = Layer::SOLID)
	{
		if (mesh.parts.empty() || mesh.parts.back().vertices.size() + 4 > 65536)
			mesh.parts.emplace_back();
		Mesh::Part &p = mesh.parts.back();
		u16 base = p.vertices.size();
		for (int k = 0; k < 4; k++) {
			float s = SHADE[face] * lit[k];
			video::SColor c = video::SColorf(colour.r * s, colour.g * s, colour.b * s, alphaOf(layer)).toSColor();
			p.vertices.emplace_back(corner[k] * BS, NORMAL[face], c, v2f(0, 0));
		}
		p.indices.insert(p.indices.end(), { base, (u16)(base + 1), (u16)(base + 2), base, (u16)(base + 2), (u16)(base + 3) });
	}

	// the horizontal face at height y over [x0, x1] × [z0, z1]
	void flat(float x0, float x1, float z0, float z1, float y, Face face, video::SColorf colour, Layer layer = Layer::SOLID)
	{
		if (face == UP)
			quad({ { x0, y, z0 }, { x0, y, z1 }, { x1, y, z1 }, { x1, y, z0 } }, face, colour, layer);
		else
			quad({ { x0, y, z0 }, { x1, y, z0 }, { x1, y, z1 }, { x0, y, z1 } }, face, colour, layer);
	}

	// the vertical face on the `face` side of [x0, x1] × [z0, z1], from y0 up to y1
	// `low` and `high`: how lit its bottom and top edges are
	void side(float x0, float x1, float z0, float z1, float y0, float y1, Face face, video::SColorf colour,
			Layer layer = Layer::SOLID, float low = 1, float high = 1)
	{
		if (y1 <= y0)
			return;
		const float lit[4] = { low, high, high, low }; // each case below runs bottom, top, top, bottom
		switch (face) {
		case EAST:  quad({ { x1, y0, z0 }, { x1, y1, z0 }, { x1, y1, z1 }, { x1, y0, z1 } }, face, lit, colour, layer); break;
		case WEST:  quad({ { x0, y0, z1 }, { x0, y1, z1 }, { x0, y1, z0 }, { x0, y0, z0 } }, face, lit, colour, layer); break;
		case NORTH: quad({ { x1, y0, z1 }, { x1, y1, z1 }, { x0, y1, z1 }, { x0, y0, z1 } }, face, lit, colour, layer); break;
		case SOUTH: quad({ { x0, y0, z0 }, { x0, y1, z0 }, { x1, y1, z0 }, { x1, y0, z0 } }, face, lit, colour, layer); break;
		default: break;
		}
	}
};

// Top faces along a row, merged where neighbours are the same height and
// colour: seas, plains and beaches become strips, not a quad a cell.
class Row
{
public:
	Row(Builder &b, float z0, float z1) : m_b(b), m_z0(z0), m_z1(z1) {}
	~Row() { flush(); }

	void add(float x0, float x1, float y, video::SColorf colour, Layer layer)
	{
		video::SColor c = colour.toSColor();
		if (m_open && m_y == y && m_colour == c && m_layer == layer && m_x1 == x0) {
			m_x1 = x1;
			return;
		}
		flush();
		m_open = true;
		m_x0 = x0;
		m_x1 = x1;
		m_y = y;
		m_colour = c;
		m_rgb = colour;
		m_layer = layer;
	}

	void flush()
	{
		if (m_open)
			m_b.flat(m_x0, m_x1, m_z0, m_z1, m_y, UP, m_rgb, m_layer);
		m_open = false;
	}

private:
	Builder &m_b;
	float m_z0, m_z1;
	bool m_open = false;
	float m_x0 = 0, m_x1 = 0, m_y = 0;
	video::SColor m_colour;
	video::SColorf m_rgb;
	Layer m_layer = Layer::SOLID;
};

struct Span
{
	float low, high;
};

// [low, high] less what `cover` hides: up to two pieces
static int subtract(Span s, Span cover, Span out[2])
{
	if (cover.high <= s.low || cover.low >= s.high) {
		out[0] = s;
		return 1;
	}
	int n = 0;
	if (cover.low > s.low)
		out[n++] = { s.low, cover.low };
	if (cover.high < s.high)
		out[n++] = { cover.high, s.high };
	return n;
}

} // namespace

Mesh mesh(const Tile &tile, const Colours &colours)
{
	Builder b;
	const s32 cell = tile.key.cell();
	static const s32 DI[4] = { 1, -1, 0, 0 }, DJ[4] = { 0, 0, 1, -1 };
	static const Face SIDE[4] = { EAST, WEST, NORTH, SOUTH };
	// faces are at block edges: a block at y fills y − ½ .. y + ½
	auto top = [](s32 y) { return (float)y + 0.5f; };
	// Under water, light falls a level per block of it (water passes light,
	// not sunlight), on the engine's light curve. At height y (a face's edge)
	// under a surface at `water`.
	auto lightAt = [](float y, s32 water) {
		s32 depth = (s32)std::ceil((float)water + 0.5f - y);
		return (float)decode_light((u8)std::clamp(LIGHT_SUN - depth, 0, (int)LIGHT_SUN)) / (float)decode_light(LIGHT_SUN);
	};

	for (s32 j = 0; j < TILE; j++) {
		float z0 = (float)(j * cell) - 0.5f, z1 = z0 + (float)cell;
		// the row's ground (or liquid) on top, and the floors under its liquids
		Row surface(b, z0, z1), floors(b, z0, z1);
		for (s32 i = 0; i < TILE; i++) {
			const Cell &c = tile.at(i, j);
			float x0 = (float)(i * cell) - 0.5f, x1 = x0 + (float)cell;
			bool wet = c.water > c.ground;

			if (!wet) {
				surface.add(x0, x1, top(c.ground), colours.face(c.top, UP), Layer::SOLID);
			} else {
				float lit = lightAt(top(c.ground), c.water);
				video::SColorf floor = colours.face(c.top, UP);
				// the liquid, seen through as the near world's is; and the floor
				// alone, for where the near world lays its own liquid over it
				surface.add(x0, x1, top(c.water), colours.through(c.liquid, floor, lit), Layer::LIQUID);
				floors.add(x0, x1, top(c.ground), video::SColorf(floor.r * lit, floor.g * lit, floor.b * lit, 1), Layer::UNDER);
			}

			for (int k = 0; k < 4; k++) {
				const Cell &n = tile.at(i + DI[k], j + DJ[k]);
				Face face = SIDE[k];
				// a cliff: the soil's bands, then rock
				if (n.ground < c.ground) {
					float low = top(n.ground), high = top(c.ground);
					float bands[3] = { top(c.top_end), top(c.filler_end), low };
					MapNode nodes[3] = { c.top, c.filler, c.rock };
					float upper = high;
					// a step under water is lit as deep as it is: the neighbour's
					// water is what lies against it
					bool under = n.water > n.ground;
					for (int band = 0; band < 3 && upper > low; band++) {
						float lower = std::max(bands[band], low);
						b.side(x0, x1, z0, z1, lower, upper, face, colours.face(nodes[band], face),
								under ? Layer::UNDER : Layer::SOLID,
								under ? lightAt(lower, n.water) : 1.0f, under ? lightAt(upper, n.water) : 1.0f);
						upper = std::min(upper, lower);
					}
				}
				// a liquid's edge, where the land beyond is lower than its surface
				if (wet) {
					float beyond = top(std::max(n.ground, n.water));
					b.side(x0, x1, z0, z1, std::max(beyond, top(c.ground)), top(c.water), face,
							colours.face(c.liquid, face), Layer::LIQUID);
				}
			}

			// the trees: boxes of their blocks, open where a neighbour covers them
			for (const Run *run : { &c.trunk, &c.canopy }) {
				if (run->empty())
					continue;
				Span s{ (float)run->low - 0.5f, top(run->high) };
				b.flat(x0, x1, z0, z1, s.high, UP, colours.face(run->node, UP));
				if (run->low > c.ground + 1)
					b.flat(x0, x1, z0, z1, s.low, DOWN, colours.face(run->node, DOWN));
				for (int k = 0; k < 4; k++) {
					const Cell &n = tile.at(i + DI[k], j + DJ[k]);
					Span pieces[4] = { s };
					int count = 1;
					Span covers[3] = { { -1e9f, top(n.ground) },
						{ (float)n.trunk.low - 0.5f, n.trunk.empty() ? -1e9f : top(n.trunk.high) },
						{ (float)n.canopy.low - 0.5f, n.canopy.empty() ? -1e9f : top(n.canopy.high) } };
					for (const Span &cover : covers) {
						Span next[4];
						int m = 0;
						for (int p = 0; p < count; p++)
							m += subtract(pieces[p], cover, next + m);
						std::copy(next, next + m, pieces);
						count = m;
					}
					for (int p = 0; p < count; p++)
						b.side(x0, x1, z0, z1, pieces[p].low, pieces[p].high, SIDE[k], colours.face(run->node, SIDE[k]));
				}
			}
		}
	}
	return std::move(b.mesh);
}

} // namespace far
