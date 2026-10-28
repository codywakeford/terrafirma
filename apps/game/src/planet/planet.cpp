// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "planet.h"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <stdexcept>
#include <string>

namespace planet {

using Place = terragen::Sphere::Place;

Planet::Planet(int32_t half, terragen::Sphere::Mapping mapping) : m_sphere(half, mapping)
{
	if (half % 8 != 0 || half > MAX_HALF)
		throw std::invalid_argument("Planet: a face is whole map blocks, at most "
			+ std::to_string(MAX_HALF) + " from its centre to its edge (" + std::to_string(half) + ")");
}

Planet::Square Planet::square(int face) const
{
	if (face < 0 || face >= 6)
		throw std::out_of_range("Planet: no face " + std::to_string(face));
	return { ORIGIN + (face % 3) * (width() + GAP), ORIGIN + (face / 3) * (width() + GAP) };
}

std::optional<int> Planet::face(int32_t x, int32_t z) const
{
	const int32_t pitch = width() + GAP;
	const int32_t dx = x - ORIGIN, dz = z - ORIGIN;
	if (dx < 0 || dz < 0)
		return std::nullopt;
	const int32_t col = dx / pitch, row = dz / pitch;
	if (col > 2 || row > 1 || dx % pitch >= width() || dz % pitch >= width())
		return std::nullopt;
	return row * 3 + col;
}

int Planet::nearest(double x, double z) const
{
	// a node's square is [x − ½, x + ½]: faces own [x0 − ½, x0 + width − ½]
	const double pitch = width() + GAP;
	const auto index = [&](double v, int most) {
		const double i = std::floor((v - ORIGIN + 0.5 + GAP / 2.0) / pitch);
		return (int)std::clamp(i, 0.0, (double)most);
	};
	return index(z, 1) * 3 + index(x, 2);
}

Place Planet::place(int face, double x, double z) const
{
	const Square s = square(face);
	// the face's centre is between its middle two nodes
	return { face, x - (s.x + half() - 0.5), z - (s.z + half() - 0.5) };
}

std::array<double, 2> Planet::at(const Place &p) const
{
	const Square s = square(p.face);
	return { p.x + s.x + half() - 0.5, p.z + s.z + half() - 0.5 };
}

std::optional<Planet::Step> Planet::halo(int face, int32_t x, int32_t z) const
{
	const Square s = square(face);
	const int32_t w = width();
	const int32_t ox = x < s.x ? -1 : x >= s.x + w ? 1 : 0;
	const int32_t oz = z < s.z ? -1 : z >= s.z + w ? 1 : 0;
	if (ox == 0 && oz == 0)
		return Step{ x, z, face, 0 };
	if (x < s.x - w || x >= s.x + 2 * w || z < s.z - w || z >= s.z + 2 * w)
		return std::nullopt;
	if (ox != 0 && oz != 0) {
		// There is no fourth corner cell, but there is also no hole. Follow
		// the source grid from its centre; carry resolves the two crossings
		// in a stable order and lands on one of the other incident faces.
		const double cx = s.x + half() - 0.5;
		const double cz = s.z + half() - 0.5;
		const std::optional<Carried> c = carry(face, cx, cz, x, z);
		if (!c)
			return std::nullopt;
		return Step{ (int32_t)std::lround(c->x), (int32_t)std::lround(c->z), c->face, c->turn };
	}
	// the node on the edge it's past, the step across, then on
	const int32_t ex = ox == 0 ? x : ox > 0 ? s.x + w - 1 : s.x;
	const int32_t ez = oz == 0 ? z : oz > 0 ? s.z + w - 1 : s.z;
	const int32_t depth = std::abs(x - ex) + std::abs(z - ez);
	if (depth > w)
		return std::nullopt; // past the neighbour face too
	Step out = step(ex, ez, ox, oz);
	const std::array<int, 2> on = turned(ox, oz, out.turn);
	out.x += on[0] * (depth - 1);
	out.z += on[1] * (depth - 1);
	return out;
}

std::optional<Planet::Carried> Planet::carry(int face, double x, double z) const
{
	const int32_t nx = (int32_t)std::lround(x), nz = (int32_t)std::lround(z);
	const std::optional<Step> h = halo(face, nx, nz);
	if (!h || h->face == face)
		return std::nullopt;
	const std::array<double, 2> in = turned(x - nx, z - nz, h->turn);
	return Carried{ h->x + in[0], h->z + in[1], h->face, h->turn };
}

std::optional<Planet::Carried> Planet::carry(int face, double from_x, double from_z,
		double to_x, double to_z) const
{
	double x = from_x, z = from_z;
	double dx = to_x - from_x, dz = to_z - from_z;
	int current_face = face;
	int total_turn = 0;
	bool crossed = false;
	constexpr double EPSILON = 1e-7;

	// A straight update cannot legitimately need this many crossings within
	// the engine's coordinate range. The bound makes corrupt inputs harmless.
	for (int crossing = 0; crossing < 64; crossing++) {
		const Square s = square(current_face);
		const double low_x = s.x - 0.5, high_x = s.x + width() - 0.5;
		const double low_z = s.z - 0.5, high_z = s.z + width() - 0.5;
		double tx = INFINITY, tz = INFINITY;
		int sx = 0, sz = 0;
		if (dx > 0) {
			tx = (high_x - x) / dx;
			sx = 1;
		} else if (dx < 0) {
			tx = (low_x - x) / dx;
			sx = -1;
		}
		if (dz > 0) {
			tz = (high_z - z) / dz;
			sz = 1;
		} else if (dz < 0) {
			tz = (low_z - z) / dz;
			sz = -1;
		}
		const double t = std::min(tx, tz);
		if (!(t >= 0 && t < 1)) {
			x += dx;
			z += dz;
			return crossed ? std::optional<Carried>(Carried{ x, z, current_face, total_turn }) : std::nullopt;
		}

		// At an exact corner there is no fourth cell. Resolve x first, then
		// the remaining movement immediately enters the third incident face.
		const bool across_x = tx <= tz;
		x += dx * t;
		z += dz * t;
		dx *= 1 - t;
		dz *= 1 - t;
		const int ox = across_x ? sx : 0;
		const int oz = across_x ? 0 : sz;
		const int32_t edge_x = ox > 0 ? s.x + width() - 1 : ox < 0 ? s.x :
				std::clamp<int32_t>((int32_t)std::lround(x), s.x, s.x + width() - 1);
		const int32_t edge_z = oz > 0 ? s.z + width() - 1 : oz < 0 ? s.z :
				std::clamp<int32_t>((int32_t)std::lround(z), s.z, s.z + width() - 1);
		const Step edge = step(edge_x, edge_z, ox, oz);

		Place p = place(current_face, x, z);
		p.x += ox * EPSILON;
		p.z += oz * EPSILON;
		const std::array<double, 2> mapped = at(m_sphere.settle(p));
		x = mapped[0];
		z = mapped[1];
		const std::array<double, 2> remaining = turned(dx, dz, edge.turn);
		dx = remaining[0];
		dz = remaining[1];
		current_face = edge.face;
		total_turn = (total_turn + edge.turn) % 4;
		crossed = true;
	}
	throw std::logic_error("Planet::carry: movement crosses too many faces");
}

std::array<double, 2> Planet::turned(double dx, double dz, int turn)
{
	for (int i = 0; i < ((turn % 4) + 4) % 4; i++) {
		const double t = dx;
		dx = -dz;
		dz = t;
	}
	return { dx, dz };
}

std::array<int32_t, 2> Planet::wrap(int face, double x, double z) const
{
	// A nearby grid carried over one edge has an exact, block-for-block
	// correspondence. Use it before the spherical projection below, which
	// is only an approximation once the source grid goes beyond its face.
	const int32_t nx = (int32_t)std::lround(x), nz = (int32_t)std::lround(z);
	if (const std::optional<Step> h = halo(face, nx, nz))
		return { h->x, h->z };
	const Place q = m_sphere.settle(place(face, x, z));
	const std::array<double, 2> m = at(q);
	// on the face, never in a gap: its nearest node
	const Square t = square(q.face);
	const auto in = [&](double v, int32_t low) {
		return std::clamp<int32_t>((int32_t)std::lround(v), low, low + width() - 1);
	};
	return { in(m[0], t.x), in(m[1], t.z) };
}

std::string Planet::glslAxes()
{
	std::ostringstream s;
	const auto vec = [&](const terragen::Sphere::Vec &v) {
		s << "vec3(" << v.x << ".0, " << v.y << ".0, " << v.z << ".0)";
	};
	s << "void planetAxes(highp float face, out highp vec3 o, out highp vec3 e, out highp vec3 n)\n{\n";
	for (int f = 0; f < 6; f++) {
		const terragen::Sphere::Axes &a = terragen::Sphere::axes(f);
		s << (f == 0 ? "\tif" : "\telse if") << " (face < " << f << ".5) { o = ";
		vec(a.out);
		s << "; e = ";
		vec(a.east);
		s << "; n = ";
		vec(a.north);
		s << "; }\n";
	}
	s << "}\n";
	return s.str();
}

std::array<int32_t, 2> Planet::equirect(double x, double z, int32_t size) const
{
	const Place p = place(nearest(x, z), x, z);
	const double half = size / 2.0;
	double wx = std::round(m_sphere.longitude(p) / (2 * M_PI) * size);
	if (wx >= half)
		wx -= size;
	const double wz = std::clamp(std::round(m_sphere.latitude(p) / M_PI * size), 1 - half, half);
	return { (int32_t)wx, (int32_t)wz };
}

double Planet::corner(double x, double z) const
{
	const terragen::Sphere::Vec d = m_sphere.direction(place(nearest(x, z), x, z));
	// the corners are (±1, ±1, ±1)/√3: the nearest is the one with d's signs
	const double cosine = (std::abs(d.x) + std::abs(d.y) + std::abs(d.z)) / std::sqrt(3.0);
	return std::acos(std::clamp(cosine, -1.0, 1.0)) * m_sphere.radius();
}

double Planet::distance(double ax, double ay, double az, double bx, double by, double bz) const
{
	const terragen::Sphere::Vec a = m_sphere.point(place(nearest(ax, az), ax, az), ay);
	const terragen::Sphere::Vec b = m_sphere.point(place(nearest(bx, bz), bx, bz), by);
	return std::hypot(a.x - b.x, a.y - b.y, a.z - b.z);
}

std::array<int, 2> Planet::turned(int dx, int dz, int turn)
{
	for (int i = 0; i < ((turn % 4) + 4) % 4; i++) {
		const int t = dx;
		dx = -dz;
		dz = t;
	}
	return { dx, dz };
}

Planet::Step Planet::step(int32_t x, int32_t z, int dx, int dz) const
{
	if (std::abs(dx) + std::abs(dz) != 1)
		throw std::invalid_argument("Planet::step: one step along the grid");
	const std::optional<int> from = face(x, z);
	if (!from)
		throw std::invalid_argument("Planet::step: (" + std::to_string(x) + ", " + std::to_string(z) + ") is on no face");
	if (face(x + dx, z + dz) == from)
		return { x + dx, z + dz, *from, 0 };

	// Off the edge: where the step crosses it (exact on both faces: they meet
	// there block for block), then half a node in on the neighbour.
	const double h = half();
	Place p = place(*from, x, z);
	if (dx != 0)
		p.x = dx * h;
	else
		p.z = dz * h;
	Place q = m_sphere.settle({ p.face, p.x + dx * 1e-9, p.z + dz * 1e-9 });
	int ix = 0, iz = 0; // the way in on the neighbour face
	if (std::abs(std::abs(q.x) - h) < std::abs(std::abs(q.z) - h)) {
		ix = q.x > 0 ? -1 : 1;
		q.x = -ix * (h - 0.5);
		q.z = std::floor(q.z) + 0.5;
	} else {
		iz = q.z > 0 ? -1 : 1;
		q.z = -iz * (h - 0.5);
		q.x = std::floor(q.x) + 0.5;
	}
	const std::array<double, 2> m = at(q);
	int turn = 0;
	while (turned(dx, dz, turn) != std::array<int, 2>{ ix, iz })
		turn++;
	return { (int32_t)std::lround(m[0]), (int32_t)std::lround(m[1]), q.face, turn };
}

} // namespace planet

namespace planet {

template <typename T>
Lens<T>::Lens(const Planet &planet, double x, double y, double z) :
	face(planet.nearest(x, z)),
	near(planet.sphere(), planet.place(face, x, z), y),
	m_x(T(x)), m_z(T(z)),
	m_origin(T(Planet::ORIGIN)), m_pitch(T(planet.width() + Planet::GAP)), m_half(T(planet.half()))
{
}

template <typename T>
typename Lens<T>::V3 Lens<T>::at(T east, T height, T north) const
{
	using std::floor;
	using std::tan;
	using std::sqrt;
	using std::clamp;
	const auto mapped = [&](T x, T z) {
		if (near.mapping != T(terragen::Sphere::Mapping::Cobe))
			return tan(x);
		const T x2 = x * x, z2 = z * z;
		const T correction = T(-0.0941) * x2 + T(0.0276) * z2 - T(0.0623) * x2 * x2 +
				T(0.0409) * x2 * z2 + T(0.0342) * z2 * z2;
		return T(0.7240) * x + T(0.2760) * x * x2 + (T(1) - x2) * x * correction;
	};
	const auto finish = [&](V3 grid, V3 physical) {
		const T distance = sqrt(physical.x * physical.x + physical.y * physical.y + physical.z * physical.z);
		T t = clamp((distance - T(16)) / T(112), T(0), T(1));
		t = t * t * (T(3) - T(2) * t);
		return V3{ grid.x + t * (physical.x - grid.x), grid.y + t * (physical.y - grid.y),
			grid.z + t * (physical.z - grid.z) };
	};
	// the face nearest the point (Planet::nearest), and where it is on it (Planet::place)
	const T qx = m_x + east, qz = m_z + north;
	const T ix = floor((qx - m_origin + T(0.5) + T(Planet::GAP) / 2) / m_pitch);
	const T iz = floor((qz - m_origin + T(0.5) + T(Planet::GAP) / 2) / m_pitch);
	const T col = clamp(ix, T(0), T(2)), row = clamp(iz, T(0), T(1));
	const int f = int(row * 3 + col);
	if (f == face) {
		const auto g = near.at(east, north, height), p = near.atPhysical(east, north, height);
		return finish({ g.x, g.y, g.z }, { p.x, p.y, p.z });
	}
	// another face: its point on the planet, seen from the eye
	const T lx = qx - (m_origin + col * m_pitch + m_half - T(0.5));
	const T lz = qz - (m_origin + row * m_pitch + m_half - T(0.5));
	const terragen::Sphere::Axes &a = terragen::Sphere::axes(f), &e = terragen::Sphere::axes(face);
	const auto v = [](const terragen::Sphere::Vec &w) { return V3{ T(w.x), T(w.y), T(w.z) }; };
	const auto unit = [](V3 w) { T n = sqrt(w.x * w.x + w.y * w.y + w.z * w.z); return V3{ w.x / n, w.y / n, w.z / n }; };
	const auto sum = [](V3 o, T s, V3 p, T t, V3 q) {
		return V3{ o.x + s * p.x + t * q.x, o.y + s * p.y + t * q.y, o.z + s * p.z + t * q.z };
	};
	const auto dot = [](V3 p, V3 q) { return p.x * q.x + p.y * q.y + p.z * q.z; };
	const V3 d = unit(sum(v(a.out), mapped(lx * near.step, lz * near.step), v(a.east),
			mapped(lz * near.step, lx * near.step), v(a.north)));
	const V3 de = unit(sum(v(e.out), mapped(near.angleEast, near.angleNorth), v(e.east),
			mapped(near.angleNorth, near.angleEast), v(e.north)));
	const T r = near.radius + height, rise = height - near.eyeHeight;
	const V3 p{ r * (d.x - de.x) + rise * de.x, r * (d.y - de.y) + rise * de.y, r * (d.z - de.z) + rise * de.z };
	const V3 local{ dot(p, v(e.east)), dot(p, v(e.out)), dot(p, v(e.north)) };
	const auto rowOf = [&](const auto &rows, int i) {
		const auto &w = rows[i];
		return w.x * local.x + w.y * local.y + w.z * local.z;
	};
	return finish({ rowOf(near.rows, 0), rowOf(near.rows, 1), rowOf(near.rows, 2) },
		{ rowOf(near.physicalRows, 0), rowOf(near.physicalRows, 1), rowOf(near.physicalRows, 2) });
}

template class Lens<double>;
template class Lens<float>;

} // namespace planet
