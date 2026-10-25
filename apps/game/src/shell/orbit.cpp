// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "orbit.h"
#include "terragen/code.h"
#include "terragen/preview.h"
#include "terragen/sun.h"
#include "terragen/world.h"
#include "util/base64.h"
#include "porting.h"
#include <CMeshBuffer.h>
#include <IVideoDriver.h>
#include <irr_ptr.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <random>
#include <string>
#include <vector>

namespace shell {
namespace {

using Vec = core::vector3df;
constexpr float PI = 3.14159265358979323846f;
constexpr int LON = 128;
constexpr int LAT = 64;
constexpr float RADIUS = 2.2f;
constexpr float EAST_SIDE = 0.8f;  // a block side's light, of its top's (as in game)
constexpr float SOUTH_SIDE = 0.65f;

struct ShadeVertex {
	video::SColor base;
	Vec normal;
	bool water;
};

struct Body {
	irr_ptr<scene::SMeshBuffer> mesh{new scene::SMeshBuffer()};
	irr_ptr<scene::SMeshBuffer> east{new scene::SMeshBuffer()};
	irr_ptr<scene::SMeshBuffer> south{new scene::SMeshBuffer()};
	std::vector<ShadeVertex> shade;
	std::vector<ShadeVertex> east_shade;
	std::vector<ShadeVertex> south_shade;
};

static Vec direction(float u, float v)
{
	float lon = u * 2 * PI;
	float lat = (0.5f - v) * PI;
	return Vec(std::cos(lat) * std::sin(lon), std::sin(lat),
			-std::cos(lat) * std::cos(lon));
}

static video::SColor rgb(int r, int g, int b, int a = 255)
{
	return video::SColor(a, r, g, b);
}

// Bodies (and the cloud shell) are closed and wound outward: their far
// halves are culled, so they can't show through whatever depth does. The
// flat glows are seen from both sides.
static void material(scene::SMeshBuffer *mesh, bool transparent = false, bool additive = false,
		bool closed = false)
{
	auto &m = mesh->getMaterial();
	m.BackfaceCulling = closed;
	m.FogEnable = false;
	m.MaterialType = additive ? video::EMT_ONETEXTURE_BLEND :
			transparent ? video::EMT_TRANSPARENT_VERTEX_ALPHA : video::EMT_SOLID;
	if (additive)
		m.MaterialTypeParam = video::pack_textureBlendFunc(video::EBF_SRC_ALPHA,
				video::EBF_ONE, video::EMFN_MODULATE_1X, video::EAS_VERTEX_COLOR);
	if (transparent || additive)
		m.ZWriteEnable = video::EZW_OFF;
}

static void quad(scene::SMeshBuffer *mesh, const std::array<Vec, 4> &points,
		video::SColor color, std::vector<ShadeVertex> *shade = nullptr, bool water = false)
{
	auto &verts = mesh->Vertices->Data;
	auto &indices = mesh->Indices->Data;
	u16 first = (u16)verts.size();
	Vec normal = (points[1] - points[0]).crossProduct(points[2] - points[0]);
	normal.normalize();
	for (const Vec &point : points) {
		verts.emplace_back(point, normal, color, core::vector2df());
		if (shade)
			shade->push_back({color, normal, water});
	}
	indices.insert(indices.end(), {first, (u16)(first + 1), (u16)(first + 2),
			first, (u16)(first + 2), (u16)(first + 3)});
}

// A cliff along the edge p–q from height `high` down to `low`, facing `out`
// (toward the lower cell). Shaded as a block's side is: lit like the top it
// belongs to, a fixed step darker (lit by its own facing, the sides turned
// from the sun went near black).
static void wall(scene::SMeshBuffer *mesh, const Vec &p, const Vec &q, float high, float low,
		const Vec &out, video::SColor top, float side, std::vector<ShadeVertex> &shade)
{
	Vec normal = (p * low - p * high).crossProduct(q * low - p * high); // as quad() makes it
	video::SColor color(255, (u32)(top.getRed() * side), (u32)(top.getGreen() * side), (u32)(top.getBlue() * side));
	if (normal.dotProduct(out) >= 0)
		quad(mesh, {p * high, p * low, q * low, q * high}, color);
	else
		quad(mesh, {q * high, q * low, p * low, p * high}, color);
	Vec up = p + q;
	up.normalize();
	for (int i = 0; i < 4; ++i)
		shade.push_back({color, up, false});
}

// One colour per biome, in terragen::Biome's order (the menu's worldgen
// preview, ui/src/worldgen/fields.ts, uses the same). Sized by its contents and
// checked: a std::array sized by Count zero-filled the two biomes added later,
// and drew them black.
static const video::SColor BIOME[] = {
	rgb(18, 52, 96), rgb(30, 84, 140), rgb(200, 220, 235),
	rgb(214, 200, 140), rgb(222, 190, 118), rgb(176, 170, 84),
	rgb(104, 160, 72), rgb(52, 116, 56), rgb(28, 120, 60),
	rgb(60, 100, 80), rgb(150, 160, 140), rgb(120, 116, 110),
	rgb(236, 240, 244), rgb(96, 140, 40), rgb(160, 150, 96),
};
static_assert(std::size(BIOME) == (size_t)terragen::Biome::Count, "a colour for every biome");

static float relief(int16_t elevation)
{
	if (elevation <= 0)
		return RADIUS;
	// Visible orbital blocks represent clusters of gameplay blocks.
	float e = std::min(1.4f, elevation / 10000.0f);
	return RADIUS + std::ceil(e * 10.0f) * 0.016f;
}

// A body's surface as cells on a lon × lat grid, row by row from the north:
// each a height (its distance from the centre) and a colour.
struct Cells
{
	int lon, lat;
	std::vector<float> height;
	std::vector<video::SColor> colour;
	std::vector<bool> wet; // takes the sun's glint
};

// The cells as blocks: a top for each, and a wall at every step between
// neighbours, facing out from the higher one and coloured from it (without
// them the body is hollow where it steps, and shows through).
static Body blocks(const Cells &cells)
{
	Body body;
	material(body.mesh.get(), false, false, true);
	material(body.east.get(), false, false, true);
	material(body.south.get(), false, false, true);
	const int LON = cells.lon, LAT = cells.lat;
	for (int y = 0; y < LAT; ++y) {
		for (int x = 0; x < LON; ++x) {
			size_t k = (size_t)y * LON + x;
			float r = cells.height[k];
			float u0 = (float)x / LON, u1 = (float)(x + 1) / LON;
			float v0 = (float)y / LAT, v1 = (float)(y + 1) / LAT;
			Vec a = direction(u0, v0), b = direction(u1, v0);
			Vec c = direction(u1, v1), d = direction(u0, v1);
			quad(body.mesh.get(), {a * r, b * r, c * r, d * r}, cells.colour[k], &body.shade, cells.wet[k]);
			Vec middle = (a + b + c + d) * 0.25f;
			size_t ek = (size_t)y * LON + (x + 1) % LON;
			float east = cells.height[ek];
			Vec toward = direction(u1 + 0.5f / LON, (v0 + v1) * 0.5f) - middle; // the east cell
			if (r > east + 0.001f)
				wall(body.east.get(), b, c, r, east, toward, cells.colour[k], EAST_SIDE, body.east_shade);
			else if (east > r + 0.001f)
				wall(body.east.get(), b, c, east, r, -toward, cells.colour[ek], EAST_SIDE, body.east_shade);
			if (y + 1 < LAT) {
				size_t sk = (size_t)(y + 1) * LON + x;
				float south = cells.height[sk];
				Vec down = direction((u0 + u1) * 0.5f, v1 + 0.5f / LAT) - middle; // the south cell
				if (r > south + 0.001f)
					wall(body.south.get(), d, c, r, south, down, cells.colour[k], SOUTH_SIDE, body.south_shade);
				else if (south > r + 0.001f)
					wall(body.south.get(), d, c, south, r, -down, cells.colour[sk], SOUTH_SIDE, body.south_shade);
			}
		}
	}
	body.mesh->recalculateBoundingBox();
	body.east->recalculateBoundingBox();
	body.south->recalculateBoundingBox();
	return body;
}

static Body planet(const terragen::Preview &preview)
{
	auto sample = [&](int x, int y) -> size_t {
		return (size_t)std::clamp(y * preview.resolution / LAT, 0, preview.resolution - 1) * preview.resolution +
				((x + LON) % LON) * preview.resolution / LON;
	};
	Cells cells{LON, LAT, std::vector<float>(LON * LAT), std::vector<video::SColor>(LON * LAT),
			std::vector<bool>(LON * LAT)};
	for (int y = 0; y < LAT; ++y) {
		for (int x = 0; x < LON; ++x) {
			size_t i = sample(x, y);
			video::SColor color = BIOME[preview.biome[i]];
			if (preview.lake[i]) color = rgb(58, 140, 205);
			// Match the preview globe's readable relief without smoothing away the
			// individual orbital terrain cells.
			float slope = (preview.elevation[sample(x - 1, std::max(y - 1, 0))] -
					preview.elevation[sample(x + 1, std::min(y + 1, LAT - 1))]) / 10000.0f;
			float tint = std::clamp(1.0f + slope * 4.0f, 0.75f, 1.25f);
			if (preview.elevation[i] < 0 && !preview.lake[i])
				tint *= std::clamp(1.0f + preview.elevation[i] / 8500.0f, 0.65f, 1.0f);
			size_t k = (size_t)y * LON + x;
			cells.height[k] = relief(preview.elevation[i]);
			cells.colour[k] = rgb(std::min(255, (int)(color.getRed() * tint)),
					std::min(255, (int)(color.getGreen() * tint)),
					std::min(255, (int)(color.getBlue() * tint)));
			cells.wet[k] = preview.elevation[i] <= 0 || preview.lake[i];
		}
	}
	return blocks(cells);
}

static uint32_t hash(uint32_t n)
{
	n ^= n >> 16; n *= 0x7feb352du; n ^= n >> 15;
	n *= 0x846ca68bu; n ^= n >> 16;
	return n;
}

static Body moon(uint32_t seed)
{
	constexpr int MLON = 24, MLAT = 12;
	Cells cells{MLON, MLAT, std::vector<float>(MLON * MLAT), std::vector<video::SColor>(MLON * MLAT),
			std::vector<bool>(MLON * MLAT)};
	for (int y = 0; y < MLAT; ++y) {
		for (int x = 0; x < MLON; ++x) {
			uint32_t h = hash(seed ^ (uint32_t)(x * 2347 + y * 977));
			int tone = 110 + (int)(h % 38);
			size_t k = (size_t)y * MLON + x;
			cells.height[k] = 0.24f + ((h >> 8) % 4) * 0.004f;
			cells.colour[k] = rgb(tone, tone - 2, tone - 6);
		}
	}
	return blocks(cells);
}

static Body clouds(const terragen::Preview &preview, uint32_t seed)
{
	Body body;
	material(body.mesh.get(), true, false, true);
	for (int y = 1; y < LAT - 1; ++y) {
		for (int x = 0; x < LON; ++x) {
			int px = x * preview.resolution / LON;
			int py = y * preview.resolution / LAT;
			float wet = preview.wet[(size_t)py * preview.resolution + px] / 100.0f;
			float shape = std::sin(x * 0.23f + seed % 29) * std::cos(y * 0.37f) +
					std::sin(x * 0.11f - y * 0.29f + seed % 17);
			if (shape + wet * 0.35f < 1.05f)
				continue;
			float u0 = (float)x / LON, u1 = (float)(x + 1) / LON;
			float v0 = (float)y / LAT, v1 = (float)(y + 1) / LAT;
			float r = RADIUS + 0.19f;
			quad(body.mesh.get(), {direction(u0, v0) * r, direction(u1, v0) * r,
					direction(u1, v1) * r, direction(u0, v1) * r},
					rgb(214, 228, 232, 85), &body.shade);
		}
	}
	body.mesh->recalculateBoundingBox();
	return body;
}

static irr_ptr<scene::SMeshBuffer> glow(float inner, float outer,
		video::SColor inner_color, video::SColor outer_color,
		const Vec &light = Vec(), bool additive = false, float waviness = 0)
{
	irr_ptr<scene::SMeshBuffer> mesh{new scene::SMeshBuffer()};
	material(mesh.get(), true, additive);
	constexpr int SEGMENTS = 96;
	for (int i = 0; i < SEGMENTS; ++i) {
		float a = 2 * PI * i / SEGMENTS, b = 2 * PI * (i + 1) / SEGMENTS;
		Vec p(std::cos(a), std::sin(a), 0), q(std::cos(b), std::sin(b), 0);
		auto edge = [&](float angle) {
			return outer * (1.0f + waviness * (0.62f * std::sin(angle * 3 + 0.4f) +
					0.38f * std::sin(angle * 7 - 0.9f)));
		};
		auto lit = [&](video::SColor color, const Vec &radial) {
			if (light.getLengthSQ() > 0) {
				float facing = std::max(0.0f, radial.dotProduct(light));
				color.setAlpha((u32)(color.getAlpha() * (0.52f + 0.48f * facing)));
			}
			return color;
		};
		auto &v = mesh->Vertices->Data;
		auto &idx = mesh->Indices->Data;
		u16 n = (u16)v.size();
		v.emplace_back(p * inner, Vec(0, 0, -1), lit(inner_color, p), core::vector2df());
		v.emplace_back(q * inner, Vec(0, 0, -1), lit(inner_color, q), core::vector2df());
		v.emplace_back(q * edge(b), Vec(0, 0, -1), lit(outer_color, q), core::vector2df());
		v.emplace_back(p * edge(a), Vec(0, 0, -1), lit(outer_color, p), core::vector2df());
		idx.insert(idx.end(), {n, (u16)(n + 1), (u16)(n + 2), n, (u16)(n + 2), (u16)(n + 3)});
	}
	mesh->recalculateBoundingBox();
	return mesh;
}

static irr_ptr<scene::SMeshBuffer> starRays(video::SColor star)
{
	irr_ptr<scene::SMeshBuffer> mesh{new scene::SMeshBuffer()};
	material(mesh.get(), true, true);
	// Uneven, faint camera glare. Long regular spokes made the Sun look stamped on.
	constexpr std::array<std::array<float, 3>, 6> RAYS = {{
		{0.17f, 0.78f, 36}, {1.39f, 0.50f, 25}, {2.78f, 0.41f, 20},
		{3.34f, 0.64f, 29}, {4.54f, 0.56f, 28}, {5.61f, 0.36f, 17},
	}};
	for (const auto &ray : RAYS) {
		float a = ray[0];
		Vec axis(std::cos(a), std::sin(a), 0);
		Vec across(-axis.Y, axis.X, 0);
		float reach = ray[1];
		int alpha = (int)ray[2];
		auto &v = mesh->Vertices->Data;
		auto &idx = mesh->Indices->Data;
		u16 n = (u16)v.size();
		v.emplace_back(axis * 0.10f - across * 0.027f, Vec(0, 0, -1),
				rgb(star.getRed(), star.getGreen(), star.getBlue(), alpha), core::vector2df());
		v.emplace_back(axis * 0.10f + across * 0.027f, Vec(0, 0, -1),
				rgb(star.getRed(), star.getGreen(), star.getBlue(), alpha), core::vector2df());
		v.emplace_back(axis * reach, Vec(0, 0, -1),
				rgb(star.getRed(), star.getGreen(), star.getBlue(), 0), core::vector2df());
		idx.insert(idx.end(), {n, (u16)(n + 1), (u16)(n + 2)});
	}
	mesh->recalculateBoundingBox();
	return mesh;
}

static std::string menuCode()
{
	static const std::string code = [] {
		std::mt19937 random((uint32_t)porting::getTimeMs());
		auto pick = [&](int lo, int hi) { return std::uniform_int_distribution<int>(lo, hi)(random); };
		std::string seed = "orbit-" + std::to_string(random());
		std::array<uint8_t, 14> params = {2, (uint8_t)pick(0, 4),
				109, 102, 128, 128, 128, 128, 128, 128, 128, 128, 130, 85};
		// Curated ranges show the generator's variety without routinely selecting
		// nearly empty water worlds or a planet with no readable coast.
		params[2] = (uint8_t)(pick(1, 8) - 1) * 255 / 7;
		params[3] = (uint8_t)pick(76, 174);   // land share ~30–68%
		params[4] = (uint8_t)pick(70, 210);   // coast
		params[5] = (uint8_t)pick(72, 220);   // mountains
		params[7] = (uint8_t)pick(100, 155);  // temperature
		params[8] = (uint8_t)pick(85, 175);   // humidity
		params[12] = (uint8_t)pick(70, 185); // axial tilt
		auto url = [](std::string s) {
			std::replace(s.begin(), s.end(), '+', '-');
			std::replace(s.begin(), s.end(), '/', '_');
			s.erase(std::remove(s.begin(), s.end(), '='), s.end());
			return s;
		};
		return "TF5." + url(base64_encode(seed)) + "." +
				url(base64_encode(std::string_view((const char *)params.data(), params.size())));
	}();
	return code;
}

static video::SColor starColor(const terragen::Star *star)
{
	static const std::array<video::SColor, 5> colors = {
		rgb(255, 128, 82), rgb(255, 190, 120), rgb(255, 244, 220),
		rgb(235, 242, 255), rgb(180, 205, 255),
	};
	return colors.at(star - terragen::STARS);
}

// Day and night on a body turned by `rotation` (degrees, as mesh() turns it:
// the same matrix, so the light stays where the sun is). Past the terminator
// it's night: a dim blue; along it, the light warms, as a sunset does.
static void illuminate(Body &body, const Vec &sun, const Vec &rotation, const video::SColor &star)
{
	constexpr float NIGHT[3] = {0.035f, 0.045f, 0.075f}; // starlight, bluish
	constexpr float WARM[3] = {0.55f, 0.15f, -0.35f};     // the terminator's tint
	core::matrix4 turn;
	turn.setRotationDegrees(rotation);
	Vec halfway = sun + Vec(0, 0, -1); halfway.normalize();
	const float tone[3] = {star.getRed() / 245.0f, star.getGreen() / 245.0f, star.getBlue() / 245.0f};
	auto light_part = [&](scene::SMeshBuffer *mesh, const std::vector<ShadeVertex> &shade) {
		auto &vertices = mesh->Vertices->Data;
		for (size_t i = 0; i < shade.size(); ++i) {
			const ShadeVertex &v = shade[i];
			Vec n = turn.transformVect(v.normal); // no translation: a rotation
			float day = n.dotProduct(sun);
			float t = std::clamp((day + 0.06f) / 0.3f, 0.0f, 1.0f);
			float lit = t * t * (3 - 2 * t); // soft across the terminator
			float band = std::exp(-std::pow((day - 0.03f) / 0.12f, 2.0f));
			float direct = lit * (0.22f + 0.85f * std::max(0.0f, day));
			float glint = v.water ? std::pow(std::max(0.0f, n.dotProduct(halfway)), 32.0f) * 64 * lit : 0;
			const u32 base[3] = {v.base.getRed(), v.base.getGreen(), v.base.getBlue()};
			int out[3];
			for (int ch = 0; ch < 3; ++ch) {
				float light = NIGHT[ch] + direct * (1 + band * WARM[ch]) * tone[ch];
				out[ch] = (int)std::clamp(base[ch] * light + glint, 0.0f, 255.0f);
			}
			vertices[i].Color = rgb(out[0], out[1], out[2], v.base.getAlpha());
		}
		if (!shade.empty()) mesh->setDirty(scene::EBT_VERTEX);
	};
	light_part(body.mesh.get(), body.shade);
	light_part(body.east.get(), body.east_shade);
	light_part(body.south.get(), body.south_shade);
}

static std::vector<u8> alphaOf(scene::SMeshBuffer *mesh)
{
	std::vector<u8> alpha;
	alpha.reserve(mesh->Vertices->Data.size());
	for (const auto &vertex : mesh->Vertices->Data)
		alpha.push_back(vertex.Color.getAlpha());
	return alpha;
}

static void stirAtmosphere(scene::SMeshBuffer *mesh, const std::vector<u8> &base, float seconds)
{
	auto &vertices = mesh->Vertices->Data;
	for (size_t i = 0; i < vertices.size(); ++i) {
		const Vec &p = vertices[i].Pos;
		float angle = std::atan2(p.Y, p.X);
		// two ripples running opposite ways round the rim, and a brighter
		// pulse circling it every ~40 s
		float ripple = 0.07f * std::sin(seconds * 0.45f + angle * 3.0f) +
				0.05f * std::sin(seconds * 0.8f - angle * 7.0f);
		float behind = std::remainder(angle - seconds * 0.16f, 2 * PI); // the pulse, -π..π
		float pulse = 0.35f * std::exp(-behind * behind * 6.0f);
		float haze = 0.9f + ripple + pulse;
		vertices[i].Color.setAlpha((u32)std::min(255.0f, base[i] * haze));
	}
	mesh->setDirty(scene::EBT_VERTEX);
}

} // namespace

struct Orbit::Impl
{
	video::IVideoDriver *driver;
	Body world;
	Body satellite;
	Body cloud;
	irr_ptr<scene::SMeshBuffer> air;
	irr_ptr<scene::SMeshBuffer> air_rim;
	irr_ptr<scene::SMeshBuffer> air_outer;
	irr_ptr<scene::SMeshBuffer> star_core;
	irr_ptr<scene::SMeshBuffer> star_inner;
	irr_ptr<scene::SMeshBuffer> star_halo;
	irr_ptr<scene::SMeshBuffer> star_rays;
	std::vector<u8> air_alpha;
	std::vector<u8> air_rim_alpha;
	std::vector<u8> air_outer_alpha;
	video::SColor star;
	uint32_t seed;
	Vec sun;
	float roll;

	explicit Impl(video::IVideoDriver *d) : driver(d),
		star(rgb(255, 244, 220)), seed(0), roll(0)
	{
		auto code = terragen::WorldCode::decode(menuCode());
		seed = code.seed_hash;
		star = starColor(code.star);
		const Vec center(-2.6f, -1.7f, 0);
		const Vec star_position(2.8f, 1.45f, 0.9f);
		sun = star_position - center;
		sun.normalize();
		float declination = terragen::declination(code.tilt, terragen::ratio(1, 6)) /
				(float)terragen::ONE;
		float xy = std::sqrt(sun.X * sun.X + sun.Y * sun.Y);
		roll = std::atan2(sun.Y, sun.X) -
				std::asin(std::clamp(std::sin(declination) / xy, -1.0f, 1.0f));
		terragen::World terrain(code);
		auto fields = terragen::preview(terrain, terragen::ratio(1, 6), 192);
		world = planet(fields);
		satellite = moon(seed);
		cloud = clouds(fields, seed);
		Vec light(sun.X, sun.Y, 0);
		light.normalize();
		air_rim = glow(RADIUS * 0.995f, RADIUS * 1.018f,
				rgb(95, 207, 250, 155), rgb(68, 176, 231, 105), light);
		air = glow(RADIUS * 1.015f, RADIUS * 1.095f,
				rgb(55, 168, 220, 115), rgb(38, 117, 191, 6), light);
		air_outer = glow(RADIUS * 1.07f, RADIUS * 1.27f,
				rgb(36, 125, 197, 32), rgb(24, 78, 130, 0), light);
		air_alpha = alphaOf(air.get());
		air_rim_alpha = alphaOf(air_rim.get());
		air_outer_alpha = alphaOf(air_outer.get());
		star_core = glow(0.0f, 0.25f, rgb(255, 255, 248, 255),
				rgb(star.getRed(), star.getGreen(), star.getBlue(), 0), Vec(), true);
		star_inner = glow(0.0f, 0.68f,
				rgb(star.getRed(), star.getGreen(), star.getBlue(), 150),
				rgb(star.getRed(), star.getGreen(), star.getBlue(), 0), Vec(), true, 0.035f);
		star_halo = glow(0.0f, 1.55f,
				rgb(star.getRed(), star.getGreen(), star.getBlue(), 44),
				rgb(star.getRed(), star.getGreen(), star.getBlue(), 0), Vec(), true, 0.075f);
		star_rays = starRays(star);
	}

	void mesh(scene::SMeshBuffer *buffer, const Vec &position, const Vec &rotation = Vec())
	{
		core::matrix4 turn, move;
		turn.setRotationDegrees(rotation);
		move.setTranslation(position);
		driver->setTransform(video::ETS_WORLD, move * turn);
		driver->setMaterial(buffer->getMaterial());
		driver->drawMeshBuffer(buffer);
	}

	// A body's tops and walls.
	void body(const Body &b, const Vec &position, const Vec &rotation)
	{
		mesh(b.mesh.get(), position, rotation);
		mesh(b.east.get(), position, rotation);
		mesh(b.south.get(), position, rotation);
	}

	void draw(float seconds)
	{
		auto size = driver->getScreenSize();
		if (!size.Width || !size.Height)
			return;
		float aspect = (float)size.Width / std::max(1u, size.Height);
		// A fine, mostly stationary star field leaves the middle clear for the UI.
		for (uint32_t i = 0; i < 650; ++i) {
			uint32_t a = hash(seed + i * 7919), b = hash(a ^ 0x715a);
			int x = (int)(a % size.Width), y = (int)(b % size.Height);
			float middle = std::abs((float)x / size.Width - 0.5f);
			if (middle < 0.19f && (b & 3) != 0)
				continue;
			float pulse = std::sin(seconds * (0.3f + (a & 7) * 0.08f) + (b & 255));
			int alpha = 95 + (int)((a >> 24) % 105) + (int)(pulse * 16);
			bool bright = (a & 63) == 0;
			int pixels = bright ? 2 : 1;
			driver->draw2DRectangle(rgb(185, 207, 227, alpha),
					core::rect<s32>(x, y, x + pixels, y + pixels));
			if (bright)
				driver->draw2DRectangle(rgb(115, 160, 195, alpha / 3),
						core::rect<s32>(x - 2, y - 2, x + 4, y + 4));
		}

		core::matrix4 projection, view;
		projection.buildProjectionMatrixPerspectiveFovLH(PI * 0.29f, aspect, 0.1f, 50.0f);
		float drift = std::sin(seconds * 0.035f) * 0.045f;
		view.buildCameraLookAtMatrixLH(Vec(drift, 0, -5.2f), Vec(0, 0, 0), Vec(0, 1, 0));
		driver->setTransform(video::ETS_PROJECTION, projection);
		driver->setTransform(video::ETS_VIEW, view);
		Vec center(-2.6f, -1.7f, 0);
		float turn = seconds * 0.012f;
		const Vec planet_turn(0, turn * 180 / PI, roll * 180 / PI);
		const Vec cloud_turn(0, seconds * 0.8f, roll * 180 / PI);
		const Vec moon_turn(0, seconds * 0.004f * 180 / PI, 0);
		illuminate(world, sun, planet_turn, star);
		illuminate(satellite, sun, moon_turn, star);
		illuminate(cloud, sun, cloud_turn, star);
		stirAtmosphere(air.get(), air_alpha, seconds);
		stirAtmosphere(air_rim.get(), air_rim_alpha, seconds);
		stirAtmosphere(air_outer.get(), air_outer_alpha, seconds);

		Vec star_position(2.8f, 1.45f, 0.9f);
		mesh(star_halo.get(), star_position);
		mesh(star_rays.get(), star_position);
		mesh(star_inner.get(), star_position);
		mesh(star_core.get(), star_position);
		mesh(air_outer.get(), center + Vec(0, 0, -0.2f));
		mesh(air.get(), center + Vec(0, 0, -0.2f));
		mesh(air_rim.get(), center + Vec(0, 0, -0.2f));
		// A tilted circle, seen nearly edge on: in front of the planet on one
		// half, behind it on the other. Its radius (3.2) stays clear of the
		// highest land (RADIUS + 0.22) and the moon's own size wherever it is.
		// Drawn in order, far to near: depth alone isn't trusted here.
		float orbit = 1.22f - seconds * 0.010f;
		float near = std::sin(orbit); // > 0: the moon is nearer than the planet
		Vec moon_position = center + Vec(3.2f * std::cos(orbit), 1.9f * near, -2.57f * near);
		if (near <= 0)
			body(satellite, moon_position, moon_turn);
		body(world, center, planet_turn);
		mesh(cloud.mesh.get(), center, cloud_turn);
		if (near > 0)
			body(satellite, moon_position, moon_turn);
	}
};

Orbit::Orbit(video::IVideoDriver *driver) : m(std::make_unique<Impl>(driver)) {}
Orbit::~Orbit() = default;
void Orbit::draw(float seconds) { m->draw(seconds); }

} // namespace shell
