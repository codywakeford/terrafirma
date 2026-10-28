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

struct CobeCoefficients
{
	double lambda, g10, g01, g20, g11, g02;
};

constexpr CobeCoefficients COBE_FORWARD{ 0.7240, -0.0941, 0.0276, -0.0623, 0.0409, 0.0342 };
constexpr CobeCoefficients COBE_INVERSE{ 1.3774, -0.2129, -0.1178, 0.0694, 0.0941, 0.0108 };

double cobe(double a, double b, const CobeCoefficients &c)
{
	const double a2 = a * a;
	const double b2 = b * b;
	const double correction = c.g10 * a2 + c.g01 * b2 + c.g20 * a2 * a2 +
			c.g11 * a2 * b2 + c.g02 * b2 * b2;
	return c.lambda * a + (1 - c.lambda) * a * a2 + (1 - a2) * a * correction;
}

std::array<double, 2> cobeInverse(double u, double v)
{
	double a = cobe(u, v, COBE_INVERSE);
	double b = cobe(v, u, COBE_INVERSE);
	for (int iteration = 0; iteration < 4; iteration++) {
		const auto derivatives = [](double x, double y) {
			const double x2 = x * x, y2 = y * y, x3 = x2 * x, y3 = y2 * y;
			const auto &c = COBE_FORWARD;
			const double s = c.g10 * x2 + c.g01 * y2 + c.g20 * x2 * x2 +
					c.g11 * x2 * y2 + c.g02 * y2 * y2;
			const double dx = 2 * c.g10 * x + 4 * c.g20 * x3 + 2 * c.g11 * x * y2;
			const double dy = 2 * c.g01 * y + 2 * c.g11 * x2 * y + 4 * c.g02 * y3;
			return std::array<double, 2>{ c.lambda + 3 * (1 - c.lambda) * x2 +
					(1 - 3 * x2) * s + (x - x3) * dx, (x - x3) * dy };
		};
		const auto du = derivatives(a, b);
		const auto dv = derivatives(b, a);
		const double fu = cobe(a, b, COBE_FORWARD) - u;
		const double fv = cobe(b, a, COBE_FORWARD) - v;
		const double determinant = du[0] * dv[0] - du[1] * dv[1];
		if (std::abs(determinant) < 1e-12)
			break;
		a -= (fu * dv[0] - fv * du[1]) / determinant;
		b -= (fv * du[0] - fu * dv[1]) / determinant;
	}
	return { std::clamp(a, -1.0, 1.0), std::clamp(b, -1.0, 1.0) };
}

} // namespace

const Sphere::Axes &Sphere::axes(int face)
{
	return FACES.at(face);
}

Sphere::Sphere(int32_t half, Mapping mapping) : m_half(half), m_radius(4 * half / M_PI), m_mapping(mapping)
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
	if (m_mapping == Mapping::Cobe) {
		const double a = p.x / m_half, b = p.z / m_half;
		const double u = cobe(a, b, COBE_FORWARD), v = cobe(b, a, COBE_FORWARD);
		return unit(f.out + u * f.east + v * f.north);
	}
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
	if (m_mapping == Mapping::Cobe) {
		const auto p = cobeInverse(dot(d, f.east) / out, dot(d, f.north) / out);
		return { face, p[0] * m_half, p[1] * m_half };
	}
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
	angleEast(T(s.mapping() == Mapping::Cobe ? eye.x / s.half() : s.angle(eye.x))),
	angleNorth(T(s.mapping() == Mapping::Cobe ? eye.z / s.half() : s.angle(eye.z))),
	eyeHeight(T(height)), radius(T(s.radius())),
	step(T((s.mapping() == Mapping::Cobe ? 1.0 : QUARTER) / s.half())), mapping(T(s.mapping()))
{
	// The grid at the eye, in the face's axes: a block east, a metre up, a block north.
	const double a = angleEast, b = angleNorth, r = s.radius() + height;
	double u, v, ux, uz, vx, vz;
	if (s.mapping() == Mapping::Cobe) {
		u = cobe(a, b, COBE_FORWARD);
		v = cobe(b, a, COBE_FORWARD);
		const auto derivatives = [](double x, double y) {
			const double x2 = x * x, y2 = y * y, x3 = x2 * x, y3 = y2 * y;
			const auto &c = COBE_FORWARD;
			const double q = c.g10 * x2 + c.g01 * y2 + c.g20 * x2 * x2 +
					c.g11 * x2 * y2 + c.g02 * y2 * y2;
			const double dx = 2 * c.g10 * x + 4 * c.g20 * x3 + 2 * c.g11 * x * y2;
			const double dy = 2 * c.g01 * y + 2 * c.g11 * x2 * y + 4 * c.g02 * y3;
			return std::array<double, 2>{ c.lambda + 3 * (1 - c.lambda) * x2 +
					(1 - 3 * x2) * q + (x - x3) * dx, (x - x3) * dy };
		};
		const auto du = derivatives(a, b), dv = derivatives(b, a);
		ux = du[0] / s.half();
		uz = du[1] / s.half();
		vx = dv[1] / s.half();
		vz = dv[0] / s.half();
	} else {
		u = std::tan(a);
		v = std::tan(b);
		ux = QUARTER / s.half() / (std::cos(a) * std::cos(a));
		uz = 0;
		vx = 0;
		vz = QUARTER / s.half() / (std::cos(b) * std::cos(b));
	}
	const Vec t{ u, 1, v };
	const double n = length(t);
	const Vec d = (1 / n) * t;
	const auto turned = [&](Vec dt) { return (r / n) * (dt - dot(d, dt) * d); };
	const Vec east = turned({ ux, 0, vx });
	const Vec north = turned({ uz, 0, vz });
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
	const Vec physicalEast = unit(east);
	const Vec physicalNorth = unit(north - dot(north, physicalEast) * physicalEast);
	physicalRows[0] = { T(physicalEast.x), T(physicalEast.y), T(physicalEast.z) };
	physicalRows[1] = { T(d.x), T(d.y), T(d.z) };
	physicalRows[2] = { T(physicalNorth.x), T(physicalNorth.y), T(physicalNorth.z) };
}

template <typename T>
typename Sphere::Lens<T>::V3 Sphere::Lens<T>::at(T east, T north, T height) const
{
	return inRows(faceAt(east, north, height), rows);
}

template <typename T>
typename Sphere::Lens<T>::V3 Sphere::Lens<T>::atPhysical(T east, T north, T height) const
{
	return inRows(faceAt(east, north, height), physicalRows);
}

template <typename T>
typename Sphere::Lens<T>::V3 Sphere::Lens<T>::inRows(const V3 &p, const std::array<V3, 3> &rows)
{
	const auto row = [&](const V3 &w) { return w.x * p.x + w.y * p.y + w.z * p.z; };
	return { row(rows[0]), row(rows[1]), row(rows[2]) };
}

template <typename T>
typename Sphere::Lens<T>::V3 Sphere::Lens<T>::faceAt(T east, T north, T height) const
{
	using std::cos;
	using std::sin;
	using std::sqrt;
	using std::tan;
	const T da = east * step, db = north * step;
	const T a = angleEast + da, b = angleNorth + db;
	T ea, eb, ta, tb, ga, gb;
	if (mapping == T(Mapping::Cobe)) {
		const auto mapped = [](T x, T y) {
			const T x2 = x * x, y2 = y * y;
			const T q = T(0.7240) * x + T(0.2760) * x * x2;
			const T c = T(-0.0941) * x2 + T(0.0276) * y2 - T(0.0623) * x2 * x2 +
					T(0.0409) * x2 * y2 + T(0.0342) * y2 * y2;
			return q + (T(1) - x2) * x * c;
		};
		ea = mapped(angleEast, angleNorth);
		eb = mapped(angleNorth, angleEast);
		ta = mapped(a, b);
		tb = mapped(b, a);
		ga = ta - ea;
		gb = tb - eb;
	} else {
		ea = tan(angleEast);
		eb = tan(angleNorth);
		ta = tan(a);
		tb = tan(b);
		ga = sin(da) / (cos(a) * cos(angleEast));
		gb = sin(db) / (cos(b) * cos(angleNorth));
	}
	const T n = sqrt(ta * ta + 1 + tb * tb), en = sqrt(ea * ea + 1 + eb * eb);
	const T grow = -(ga * (ea + ta) + gb * (eb + tb)) / (n * en * (n + en));
	const T r = radius + height, rise = height - eyeHeight;
	return { r * (ga / n + ea * grow) + rise * ea / en, r * grow + rise / en,
		r * (gb / n + eb * grow) + rise * eb / en };
}

template class Sphere::Lens<double>;
template class Sphere::Lens<float>;

} // namespace terragen
