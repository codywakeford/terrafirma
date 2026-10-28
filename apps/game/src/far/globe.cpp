// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "far/globe.h"
#include "far/terrain.h"
#include <algorithm>
#include <cmath>

namespace far {

namespace {

using Vec = Globe::Vec;

// Each face: its outward normal n and its x axis a; its z axis is a × n, so
// a tile's (x, up, z) are the planet's (a, n, a × n): turned, not mirrored.
struct Face
{
	Vec n, a;
};
const Face FACES[6] = {
	{ { 1, 0, 0 }, { 0, 0, 1 } },
	{ { -1, 0, 0 }, { 0, 0, -1 } },
	{ { 0, 1, 0 }, { 1, 0, 0 } },
	{ { 0, -1, 0 }, { 1, 0, 0 } },
	{ { 0, 0, 1 }, { -1, 0, 0 } },
	{ { 0, 0, -1 }, { 1, 0, 0 } },
};

Vec cross(const Vec &u, const Vec &v)
{
	return { u.y * v.z - u.z * v.y, u.z * v.x - u.x * v.z, u.x * v.y - u.y * v.x };
}

} // namespace

Globe::Globe(double radius) : m_radius(radius), m_top(0)
{
	// the finest cells about a block wide: a face's edge is a quarter turn
	while ((TILE << m_top) < M_PI / 2 * radius)
		m_top++;
	m_cells = TILE << m_top;
}

Vec Globe::direction(int face, double fx, double fz) const
{
	const Face &f = FACES[face];
	const Vec b = cross(f.a, f.n);
	// equal angles along each axis: cells near squares to the face's corners
	const double u = std::tan((fx / m_cells * 2 - 1) * M_PI / 4), v = std::tan((fz / m_cells * 2 - 1) * M_PI / 4);
	const Vec d{ f.n.x + u * f.a.x + v * b.x, f.n.y + u * f.a.y + v * b.y, f.n.z + u * f.a.z + v * b.z };
	const double len = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
	return { d.x / len, d.y / len, d.z / len };
}

std::array<double, 2> Globe::place(const Vec &d)
{
	return { std::asin(std::clamp(d.y, -1.0, 1.0)), std::atan2(d.z, d.x) };
}

Vec Globe::point(const TileKey &key, double x, double y, double z) const
{
	// a cell centred on whole numbers: its corner at −½; a block's top at y + ½
	const double span = key.span();
	const Vec d = direction(key.face, key.x * span + x + 0.5, key.z * span + z + 0.5);
	const double r = m_radius + y + 0.5;
	return { d.x * r, d.y * r, d.z * r };
}

Vec Globe::middle(const TileKey &key) const
{
	const double half = key.span() / 2.0 - 0.5;
	return point(key, half, -0.5, half);
}

double Globe::diagonal(const TileKey &key) const
{
	const double far = key.span() - 0.5;
	const Vec a = point(key, -0.5, -0.5, -0.5), b = point(key, far, -0.5, far);
	return std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y) + (a.z - b.z) * (a.z - b.z));
}

} // namespace far
