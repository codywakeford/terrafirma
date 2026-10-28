// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "far/layout.h"
#include "client/atlas_view.h"
#include "client/clientmap.h"
#include "constants.h"
#include "util/numeric.h"
#include <cmath>

namespace far {

namespace {

// The coarsest level on a cube planet's net: 32 · 2⁹ = 16 384 blocks, the largest world.
constexpr s32 NET_TOP = 9;

float span(s32 level)
{
	return (float)(TILE << level);
}

/*
	A cube planet's tiles: squares of its net (the map), drawn by the sphere
	lens shader as its near world is; the mask by map-block column round the
	eye, a column the near world's only once it draws all of its surface.
*/
class NetLayout : public Layout
{
public:
	NetLayout(const planet::Planet &planet, s16 water_level) : m_planet(planet), m_water_level(water_level) {}

	std::unique_ptr<Generator> generator(const Horizon &horizon) const override
	{
		return std::make_unique<Generator>(horizon, std::optional<planet::Planet>(m_planet));
	}
	Mesh mesh(const Tile &tile, const Colours &colours) const override
	{
		return far::mesh(tile, colours, m_planet);
	}
	const char *shader() const override { return "far_shader"; }
	double radius() const override { return m_planet.sphere().radius(); }

	void look(v3f eye, ClientMap &) override
	{
		m_altitude = std::max(eye.Y - m_water_level, 0.0f);
		m_eye_point = m_planet.sphere().point(m_planet.place(m_planet.nearest(eye.X, eye.Z), eye.X, eye.Z), 0);
		m_mask_centre = getNodeBlockPos(floatToInt(eye * BS, BS));
	}
	float altitude() const override { return m_altitude; }

	std::vector<TileKey> roots(float range) const override
	{
		s32 top = 0;
		while (top < NET_TOP && span(top) < range / 2)
			top++;
		// the whole net: distance() says which tiles are in reach
		const float pitch = (float)(m_planet.width() + planet::Planet::GAP), low = (float)planet::Planet::ORIGIN;
		const s32 lo = (s32)std::floor(low / span(top));
		const s32 hi_x = (s32)std::floor((low + 3 * pitch) / span(top));
		const s32 hi_z = (s32)std::floor((low + 2 * pitch) / span(top));
		std::vector<TileKey> out;
		for (s32 z = lo; z <= hi_z; z++)
			for (s32 x = lo; x <= hi_x; x++)
				out.push_back({ top, x, z });
		return out;
	}

	float distance(const TileKey &key) const override
	{
		float s = span(key.level), x0 = key.x * s - 0.5f, z0 = key.z * s - 0.5f;
		// over the planet: from the eye to the tile's middle, less its half diagonal
		const float cx = x0 + 0.5f * s, cz = z0 + 0.5f * s;
		const int face = m_planet.nearest(cx, cz);
		const terragen::Sphere::Vec p = m_planet.sphere().point(m_planet.place(face, cx, cz), 0);
		const float d = (float)std::hypot(p.x - m_eye_point.x, p.y - m_eye_point.y, p.z - m_eye_point.z);
		return std::max(d - 0.7072f * s, 0.0f);
	}

	std::optional<core::matrix4> transform(const TileKey &key, v3f origin) const override
	{
		const float s = span(key.level);
		core::matrix4 world;
		world.setTranslation(v3f(key.x * s * BS + origin.X, origin.Y, key.z * s * BS + origin.Z));
		return world;
	}

	v2f maskCorner(v3f origin) const override
	{
		return v2f(((float)(m_mask_centre.X - MASK / 2) * MAP_BLOCKSIZE - 0.5f) * BS + origin.X,
				((float)(m_mask_centre.Z - MASK / 2) * MAP_BLOCKSIZE - 0.5f) * BS + origin.Z);
	}
	std::optional<v2s32> maskColumn(v3s16 block, v3f) const override
	{
		return v2s32(block.X - m_mask_centre.X + MASK / 2, block.Z - m_mask_centre.Z + MASK / 2);
	}
	std::optional<u32> surfaceColumn(s32 u, s32 v) const override
	{
		return columnKey(m_mask_centre.X - MASK / 2 + u, m_mask_centre.Z - MASK / 2 + v);
	}

	// From lowest ground to the top of the highest water or tree, per map-block
	// column a tile covers: what the near world must draw before the far view
	// can step aside there. Tiles coarser than a map block don't say.
	std::vector<Surface> surfaces(const Tile &tile) const override
	{
		std::vector<Surface> out;
		const s32 cell = tile.key.cell();
		if (cell > MAP_BLOCKSIZE)
			return out;
		const s32 per = MAP_BLOCKSIZE / cell, columns = TILE / per;
		const s32 bx0 = tile.key.x * tile.key.span() / MAP_BLOCKSIZE, bz0 = tile.key.z * tile.key.span() / MAP_BLOCKSIZE;
		for (s32 bj = 0; bj < columns; bj++)
		for (s32 bi = 0; bi < columns; bi++) {
			Surface s{ (s16)(bx0 + bi), (s16)(bz0 + bj), INT32_MAX, INT32_MIN, (u8)tile.key.level };
			for (s32 j = bj * per; j < (bj + 1) * per; j++)
			for (s32 i = bi * per; i < (bi + 1) * per; i++) {
				const Cell &c = tile.at(i, j);
				s.low = std::min(s.low, c.ground);
				s.high = std::max({ s.high, c.ground, c.water, c.canopy.empty() ? c.ground : c.canopy.high });
			}
			out.push_back(s);
		}
		return out;
	}

	bool covered(const TileKey &key, const std::vector<u8> &near) const override
	{
		if (near.empty() || key.level > 3)
			return false;
		const s32 s = TILE << key.level, half = MASK / 2;
		const s32 bx0 = getContainerPos(key.x * s, MAP_BLOCKSIZE), bx1 = getContainerPos(key.x * s + s - 1, MAP_BLOCKSIZE);
		const s32 bz0 = getContainerPos(key.z * s, MAP_BLOCKSIZE), bz1 = getContainerPos(key.z * s + s - 1, MAP_BLOCKSIZE);
		for (s32 bz = bz0; bz <= bz1; bz++)
		for (s32 bx = bx0; bx <= bx1; bx++) {
			const s32 u = bx - m_mask_centre.X + half, v = bz - m_mask_centre.Z + half;
			if (u < 0 || u >= MASK || v < 0 || v >= MASK || !near[(size_t)v * MASK + u])
				return false;
		}
		return true;
	}

private:
	const planet::Planet m_planet;
	const s16 m_water_level;
	float m_altitude = 0;
	terragen::Sphere::Vec m_eye_point{}; // the eye over the sea, m from the centre
	v3s16 m_mask_centre;                 // map block the mask is centred on
};

/*
	An atlas's tiles: a globe's (far/globe.h), each built in true positions
	round its middle and drawn as the near world's blocks are (AtlasView: the
	eye's one map, moved to the middle), so the two are one surface. The mask
	in the eye's scene, by map-block-wide columns: a column is the near
	world's where it draws a block.
*/
class GlobeLayout : public Layout
{
public:
	GlobeLayout(const planet::Atlas &atlas, s16 water_level) :
		m_atlas(atlas), m_globe(atlas.atlas().params().radius), m_water_level(water_level)
	{
	}

	std::unique_ptr<Generator> generator(const Horizon &horizon) const override
	{
		return std::make_unique<Generator>(horizon, m_atlas);
	}

	// The tile's mesh as on a map, each vertex then put on the planet: a
	// tile's (x, y, z) at its place on the globe, round its middle; its
	// height over the sea (blocks) in the first texture coordinate, for
	// the haze.
	Mesh mesh(const Tile &tile, const Colours &colours) const override
	{
		Mesh m = far::mesh(tile, colours, std::nullopt);
		const TileKey &key = tile.key;
		const Globe::Vec mid = m_globe.middle(key);
		const float half = key.span() / 2.0f - 0.5f;
		// the tile's own axes at its middle, for the normals
		const auto axis = [&](double dx, double dy, double dz) {
			const Globe::Vec a = m_globe.point(key, half + dx, -0.5 + dy, half + dz), b = m_globe.point(key, half - dx, -0.5 - dy, half - dz);
			return v3f(a.x - b.x, a.y - b.y, a.z - b.z).normalize();
		};
		const v3f ex = axis(0.5, 0, 0), ey = axis(0, 0.5, 0), ez = axis(0, 0, 0.5);
		for (Mesh::Part &part : m.parts)
			for (video::S3DVertex &v : part.vertices) {
				const v3f t = v.Pos / BS;
				const Globe::Vec p = m_globe.point(key, t.X, t.Y, t.Z);
				v.Pos = v3f(p.x - mid.x, p.y - mid.y, p.z - mid.z) * BS;
				v.Normal = (ex * v.Normal.X + ey * v.Normal.Y + ez * v.Normal.Z).normalize();
				v.TCoords = v2f(t.Y + 0.5f - (m_water_level + 1), 0);
			}
		return m;
	}
	const char *shader() const override { return "far_globe_shader"; }
	double radius() const override { return m_globe.radius(); }

	void look(v3f eye, ClientMap &map) override
	{
		m_view = map.atlasView();
		m_eye = eye;
		const auto &frame = m_view ? m_view->eye() : std::nullopt;
		if (!frame)
			return; // the eye has no grid this frame: nothing is drawn from it
		const Globe::Vec &o = frame->origin;
		const double r = std::sqrt(o.x * o.x + o.y * o.y + o.z * o.z);
		const double sea = m_globe.radius() + m_water_level + 1;
		m_altitude = (float)std::max(r - sea, 0.0);
		m_eye_point = { o.x / r * sea, o.y / r * sea, o.z / r * sea };
	}
	float altitude() const override { return m_altitude; }

	std::vector<TileKey> roots(float) const override
	{
		std::vector<TileKey> out;
		for (s32 face = 0; face < 6; face++)
			out.push_back({ m_globe.top(), 0, 0, face });
		return out;
	}

	float distance(const TileKey &key) const override
	{
		const Globe::Vec p = m_globe.middle(key);
		const double sea = m_globe.radius() + m_water_level + 1, r = m_globe.radius();
		const double d = std::hypot(p.x * sea / r - m_eye_point.x, p.y * sea / r - m_eye_point.y, p.z * sea / r - m_eye_point.z);
		return (float)std::max(d - m_globe.diagonal(key) / 2, 0.0);
	}

	std::optional<core::matrix4> transform(const TileKey &key, v3f) const override
	{
		return m_view ? m_view->place(m_globe.middle(key)) : std::nullopt;
	}

	v2f maskCorner(v3f origin) const override
	{
		const float half = MASK / 2 * MAP_BLOCKSIZE * BS;
		return v2f(origin.X + m_eye.X * BS - half, origin.Z + m_eye.Z * BS - half);
	}
	std::optional<v2s32> maskColumn(v3s16 block, v3f origin) const override
	{
		const core::matrix4 *m = m_view ? m_view->transform(block) : nullptr;
		if (!m)
			return std::nullopt;
		// the block's middle, where it's drawn
		const v3f at = m->getTranslation();
		const v2f corner = maskCorner(origin);
		const float w = MAP_BLOCKSIZE * BS;
		return v2s32((s32)std::floor((at.X - corner.X) / w), (s32)std::floor((at.Z - corner.Y) / w));
	}
	std::optional<u32> surfaceColumn(s32, s32) const override { return std::nullopt; }
	std::vector<Surface> surfaces(const Tile &) const override { return {}; }
	bool covered(const TileKey &, const std::vector<u8> &) const override { return false; }

private:
	const planet::Atlas m_atlas;
	const Globe m_globe;
	const s16 m_water_level;
	AtlasView *m_view = nullptr; // the near world's, this frame
	v3f m_eye;                   // map nodes
	float m_altitude = 0;
	Globe::Vec m_eye_point{};    // the eye over the sea, blocks from the centre
};

} // namespace

std::unique_ptr<Layout> netLayout(const planet::Planet &planet, s16 water_level)
{
	return std::make_unique<NetLayout>(planet, water_level);
}

std::unique_ptr<Layout> globeLayout(const planet::Atlas &atlas, s16 water_level)
{
	return std::make_unique<GlobeLayout>(atlas, water_level);
}

} // namespace far
