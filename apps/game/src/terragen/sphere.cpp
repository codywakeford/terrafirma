// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "sphere.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace terragen {

using Vec = Sphere::Vec;

namespace {

constexpr double QUARTER = 0.785398163397448309616; // π/4: a face's centre to its edge

// east × north = −out on every face, so the six grids agree on which way
// round they are.
using Axes = Sphere::Axes;

constexpr std::array<Axes, 6> FACES{ {
	{ { 1, 0, 0 }, { 0, 0, 1 }, { 0, 1, 0 } },
	{ { 0, 0, 1 }, { -1, 0, 0 }, { 0, 1, 0 } },
	{ { -1, 0, 0 }, { 0, 0, -1 }, { 0, 1, 0 } },
	{ { 0, 0, -1 }, { 1, 0, 0 }, { 0, 1, 0 } },
	{ { 0, 1, 0 }, { 0, 0, 1 }, { -1, 0, 0 } },
	{ { 0, -1, 0 }, { 0, 0, 1 }, { 1, 0, 0 } },
} };

Vec operator+(Vec a, Vec b) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
Vec operator-(Vec a, Vec b) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
Vec operator*(double k, Vec a) { return { k * a.x, k * a.y, k * a.z }; }
double dot(Vec a, Vec b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
double length(Vec a) { return std::sqrt(dot(a, a)); }
Vec unit(Vec a) { return (1 / length(a)) * a; }

} // namespace

const Sphere::Axes &Sphere::axes(int face)
{
	return FACES.at(face);
}

Sphere::Sphere(int32_t half) : m_half(half), m_radius(4 * half / M_PI)
{
	if (half <= 0)
		throw std::invalid_argument("Sphere: a face needs blocks");
}

double Sphere::angle(double blocks) const
{
	return blocks * QUARTER / m_half;
}

Vec Sphere::direction(const Place &p) const
{
	const Axes &f = FACES.at(p.face);
	return unit(f.out + std::tan(angle(p.x)) * f.east + std::tan(angle(p.z)) * f.north);
}

Vec Sphere::point(const Place &p, double height) const
{
	return (m_radius + height) * direction(p);
}

Sphere::Place Sphere::place(const Vec &d) const
{
	if (length(d) == 0)
		throw std::invalid_argument("Sphere: no direction");
	int face = 0;
	for (int i = 1; i < 6; i++)
		if (dot(d, FACES[i].out) > dot(d, FACES[face].out))
			face = i;
	const Axes &f = FACES[face];
	const double out = dot(d, f.out);
	return { face, std::atan(dot(d, f.east) / out) / QUARTER * m_half,
		std::atan(dot(d, f.north) / out) / QUARTER * m_half };
}

Sphere::Frame Sphere::frame(const Place &p) const
{
	const Vec up = direction(p);
	const Vec ahead = direction({ p.face, p.x + 1e-3, p.z }) - direction({ p.face, p.x - 1e-3, p.z });
	const Vec side = direction({ p.face, p.x, p.z + 1e-3 }) - direction({ p.face, p.x, p.z - 1e-3 });
	return { unit(ahead - dot(ahead, up) * up), up, unit(side - dot(side, up) * up) };
}

double Sphere::latitude(const Place &p) const
{
	return std::asin(std::clamp(direction(p).y, -1.0, 1.0));
}

double Sphere::longitude(const Place &p) const
{
	const Vec d = direction(p);
	return std::atan2(d.z, d.x);
}

std::array<double, 2> Sphere::cellSides() const
{
	// One face's blocks, sampled: the sides of each, across and along.
	double low = INFINITY, high = 0;
	const int32_t step = std::max<int32_t>(1, m_half / 64);
	for (int32_t z = -m_half; z < m_half; z += step)
	for (int32_t x = -m_half; x < m_half; x += step) {
		const Vec c = point({ 0, x + 0.0, z + 0.0 }, 0);
		for (const Vec &n : { point({ 0, x + 1.0, z + 0.0 }, 0), point({ 0, x + 0.0, z + 1.0 }, 0) }) {
			const double side = length(n - c);
			low = std::min(low, side);
			high = std::max(high, side);
		}
	}
	return { low, high };
}

/*
	The lens. A point's direction from the centre, in the face's own axes
	(east, out, north), is t/|t| with t = (tan a, 1, tan b). From the eye's
	direction te/|te| the difference is

		t/|t| − te/|te| = (t − te)/|t| + te (|te|² − |t|²) / (|t| |te| (|t| + |te|))

	where tan a − tan ae = sin(a − ae) / (cos a cos ae): every term is a
	small number found from small numbers, never the difference of two big
	ones. Scaled by the radius and put through the grid's frame at the eye
	(inverted, so a block next to the eye is a block next to it), that's
	the point as the eye sees it.
*/
template <typename T>
Sphere::Lens<T>::Lens(const Sphere &s, const Place &eye, double height) :
	angleEast(T(s.angle(eye.x))), angleNorth(T(s.angle(eye.z))), eyeHeight(T(height)),
	radius(T(s.radius())), step(T(QUARTER / s.half()))
{
	// The grid at the eye, in the face's axes: a block east, a metre up, a block north.
	const double a = s.angle(eye.x), b = s.angle(eye.z), r = s.radius() + height;
	const Vec t{ std::tan(a), 1, std::tan(b) };
	const double n = length(t);
	const Vec d = (1 / n) * t;
	const auto turned = [&](Vec dt) { return (r * QUARTER / s.half() / n) * (dt - dot(d, dt) * d); };
	const Vec east = turned({ 1 / (std::cos(a) * std::cos(a)), 0, 0 });
	const Vec north = turned({ 0, 0, 1 / (std::cos(b) * std::cos(b)) });
	// Its inverse, by cofactors: columns east, up, north.
	const Vec c[3] = { east, d, north };
	const double det = dot(c[0], Vec{ c[1].y * c[2].z - c[1].z * c[2].y, c[1].z * c[2].x - c[1].x * c[2].z,
		c[1].x * c[2].y - c[1].y * c[2].x });
	if (std::abs(det) < 1e-12)
		throw std::logic_error("Sphere::Lens: the grid folds at the eye");
	for (int i = 0; i < 3; i++) {
		const Vec &p = c[(i + 1) % 3], &q = c[(i + 2) % 3];
		const Vec row{ (p.y * q.z - p.z * q.y) / det, (p.z * q.x - p.x * q.z) / det, (p.x * q.y - p.y * q.x) / det };
		rows[i] = { T(row.x), T(row.y), T(row.z) };
	}
}

template <typename T>
typename Sphere::Lens<T>::V3 Sphere::Lens<T>::at(T east, T north, T height) const
{
	using std::cos;
	using std::sin;
	using std::sqrt;
	using std::tan;
	const T da = east * step, db = north * step;
	const T a = angleEast + da, b = angleNorth + db;
	const T ea = tan(angleEast), eb = tan(angleNorth), ta = tan(a), tb = tan(b);
	const T ga = sin(da) / (cos(a) * cos(angleEast)), gb = sin(db) / (cos(b) * cos(angleNorth));
	const T n = sqrt(ta * ta + 1 + tb * tb), en = sqrt(ea * ea + 1 + eb * eb);
	const T grow = -(ga * (ea + ta) + gb * (eb + tb)) / (n * en * (n + en));
	const T r = radius + height, rise = height - eyeHeight;
	const V3 p{ r * (ga / n + ea * grow) + rise * ea / en, r * grow + rise / en, r * (gb / n + eb * grow) + rise * eb / en };
	const auto row = [&](const V3 &w) { return w.x * p.x + w.y * p.y + w.z * p.z; };
	return { row(rows[0]), row(rows[1]), row(rows[2]) };
}

template class Sphere::Lens<double>;
template class Sphere::Lens<float>;

} // namespace terragen
