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

static void material(scene::SMeshBuffer *mesh, bool transparent = false, bool additive = false)
{
	auto &m = mesh->getMaterial();
	m.BackfaceCulling = false;
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

static const std::array<video::SColor, (int)terragen::Biome::Count> BIOME = {
	rgb(18, 52, 96), rgb(30, 84, 140), rgb(200, 220, 235),
	rgb(214, 200, 140), rgb(222, 190, 118), rgb(176, 170, 84),
	rgb(104, 160, 72), rgb(52, 116, 56), rgb(28, 120, 60),
	rgb(60, 100, 80), rgb(150, 160, 140), rgb(120, 116, 110),
	rgb(236, 240, 244),
};

static float relief(int16_t elevation)
{
	if (elevation <= 0)
		return RADIUS;
	// Visible orbital blocks represent clusters of gameplay blocks.
	float e = std::min(1.4f, elevation / 10000.0f);
	return RADIUS + std::ceil(e * 10.0f) * 0.016f;
}

static Body planet(const terragen::Preview &preview)
{
	Body body;
	material(body.mesh.get());
	material(body.east.get());
	material(body.south.get());
	auto sample = [&](int x, int y) -> size_t {
		return (size_t)std::clamp(y * preview.resolution / LAT, 0, preview.resolution - 1) * preview.resolution +
				((x + LON) % LON) * preview.resolution / LON;
	};
	for (int y = 0; y < LAT; ++y) {
		for (int x = 0; x < LON; ++x) {
			size_t i = sample(x, y);
			float r = relief(preview.elevation[i]);
			uint8_t biome = preview.biome[i];
			video::SColor color = BIOME.at(biome);
			if (preview.lake[i]) color = rgb(58, 140, 205);
			// Match the preview globe's readable relief without smoothing away the
			// individual orbital terrain cells.
			float slope = (preview.elevation[sample(x - 1, std::max(y - 1, 0))] -
					preview.elevation[sample(x + 1, std::min(y + 1, LAT - 1))]) / 10000.0f;
			float tint = std::clamp(1.0f + slope * 4.0f, 0.75f, 1.25f);
			if (preview.elevation[i] < 0 && !preview.lake[i])
				tint *= std::clamp(1.0f + preview.elevation[i] / 8500.0f, 0.65f, 1.0f);
			color = rgb(std::min(255, (int)(color.getRed() * tint)),
					std::min(255, (int)(color.getGreen() * tint)),
					std::min(255, (int)(color.getBlue() * tint)));
			float u0 = (float)x / LON, u1 = (float)(x + 1) / LON;
			float v0 = (float)y / LAT, v1 = (float)(y + 1) / LAT;
			Vec a = direction(u0, v0), b = direction(u1, v0);
			Vec c = direction(u1, v1), d = direction(u0, v1);
			bool water = preview.elevation[i] <= 0 || preview.lake[i];
			quad(body.mesh.get(), {a * r, b * r, c * r, d * r}, color, &body.shade, water);
			// Exposed sides make terrain levels read as blocks at this distance.
			float east = relief(preview.elevation[sample(x + 1, y)]);
			float south = relief(preview.elevation[sample(x, std::min(y + 1, LAT - 1))]);
			if (r > east + 0.001f)
				quad(body.east.get(), {b * r, b * east, c * east, c * r}, color, &body.east_shade);
			if (r > south + 0.001f)
				quad(body.south.get(), {d * r, c * r, c * south, d * south}, color, &body.south_shade);
		}
	}
	body.mesh->recalculateBoundingBox();
	body.east->recalculateBoundingBox();
	body.south->recalculateBoundingBox();
	return body;
}

static uint32_t hash(uint32_t n)
{
	n ^= n >> 16; n *= 0x7feb352du; n ^= n >> 15;
	n *= 0x846ca68bu; n ^= n >> 16;
	return n;
}

static Body moon(uint32_t seed)
{
	Body body;
	material(body.mesh.get());
	constexpr int MLON = 24, MLAT = 12;
	for (int y = 0; y < MLAT; ++y) {
		for (int x = 0; x < MLON; ++x) {
			uint32_t h = hash(seed ^ (uint32_t)(x * 2347 + y * 977));
			float r = 0.24f + ((h >> 8) % 4) * 0.004f;
			int tone = 110 + (int)(h % 38);
			video::SColor color = rgb(tone, tone - 2, tone - 6);
			float u0 = (float)x / MLON, u1 = (float)(x + 1) / MLON;
			float v0 = (float)y / MLAT, v1 = (float)(y + 1) / MLAT;
			quad(body.mesh.get(), {direction(u0, v0) * r, direction(u1, v0) * r,
					direction(u1, v1) * r, direction(u0, v1) * r}, color, &body.shade);
		}
	}
	body.mesh->recalculateBoundingBox();
	return body;
}

static Body clouds(const terragen::Preview &preview, uint32_t seed)
{
	Body body;
	material(body.mesh.get(), true);
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

static void illuminate(Body &body, const Vec &sun, float spin,
		const video::SColor &star, float roll = 0)
{
	float c = std::cos(spin), s = std::sin(spin);
	float cr = std::cos(roll), sr = std::sin(roll);
	Vec halfway = sun + Vec(0, 0, -1); halfway.normalize();
	auto light_part = [&](scene::SMeshBuffer *mesh, const std::vector<ShadeVertex> &shade) {
		auto &vertices = mesh->Vertices->Data;
		for (size_t i = 0; i < shade.size(); ++i) {
			const ShadeVertex &v = shade[i];
			Vec turned(c * v.normal.X + s * v.normal.Z, v.normal.Y,
					-s * v.normal.X + c * v.normal.Z);
			Vec n(cr * turned.X - sr * turned.Y, sr * turned.X + cr * turned.Y, turned.Z);
			float day = std::max(0.0f, n.dotProduct(sun));
			float light = 0.25f + day * 0.91f;
			float glint = v.water ? std::pow(std::max(0.0f, n.dotProduct(halfway)), 32.0f) * 64 : 0;
			vertices[i].Color = rgb(
					(int)std::min(255.0f, v.base.getRed() * light * star.getRed() / 245.0f + glint),
					(int)std::min(255.0f, v.base.getGreen() * light * star.getGreen() / 245.0f + glint),
					(int)std::min(255.0f, v.base.getBlue() * light * star.getBlue() / 245.0f + glint),
					v.base.getAlpha());
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
		float haze = 0.94f + 0.06f * std::sin(seconds * 0.14f + angle * 3.0f);
		vertices[i].Color.setAlpha((u32)(base[i] * haze));
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
		illuminate(world, sun, turn, star, roll);
		illuminate(satellite, sun, seconds * 0.004f, star);
		illuminate(cloud, sun, seconds * 0.8f * PI / 180, star, roll);
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
		mesh(world.mesh.get(), center, Vec(0, turn * 180 / PI, roll * 180 / PI));
		mesh(world.east.get(), center, Vec(0, turn * 180 / PI, roll * 180 / PI));
		mesh(world.south.get(), center, Vec(0, turn * 180 / PI, roll * 180 / PI));
		mesh(cloud.mesh.get(), center, Vec(0, seconds * 0.8f, roll * 180 / PI));
		// The projected orbit stays left of the menu. Its lower arc passes
		// behind the planet with ordinary depth testing before reappearing.
		float orbit = 1.22f - seconds * 0.010f;
		Vec moon_position = center + Vec(1.55f * std::cos(orbit),
				3.5f * std::sin(orbit), -0.75f * std::sin(orbit));
		mesh(satellite.mesh.get(), moon_position,
				Vec(0, seconds * 0.004f * 180 / PI, 0));
	}
};

Orbit::Orbit(video::IVideoDriver *driver) : m(std::make_unique<Impl>(driver)) {}
Orbit::~Orbit() = default;
void Orbit::draw(float seconds) { m->draw(seconds); }

} // namespace shell
