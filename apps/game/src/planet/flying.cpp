// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "flying.h"
#include <cmath>

namespace planet {

using Place = terragen::Sphere::Place;

// The orbit model's frame names terragen::Sphere's axes the other way round:
// both have y to the north pole; longitude 0 is the sphere's x and the
// orbit's z, 90° east the sphere's z and the orbit's x.
static orbit::Vec toOrbitAxes(const terragen::Sphere::Vec &v)
{
	return { v.z, v.y, v.x };
}

static terragen::Sphere::Vec toSphereAxes(const orbit::Vec &v)
{
	return { v.z, v.y, v.x };
}

// A vector along a place's grid directions (east and north are square to up,
// but skewed toward each other off the centre lines): solved, not projected.
static orbit::Vec along(const orbit::Frame &f, const orbit::Vec &v)
{
	const double up = v.dot(f.up);
	const orbit::Vec t = v - f.up * up;
	const double skew = f.east.dot(f.north), a = t.dot(f.east), b = t.dot(f.north);
	const double d = 1 - skew * skew;
	return { (a - skew * b) / d, up, (b - skew * a) / d };
}

orbit::Frame Flying::frame(const Place &p) const
{
	const terragen::Sphere::Frame f = m_planet.sphere().frame(p);
	return { toOrbitAxes(f.east), toOrbitAxes(f.up), toOrbitAxes(f.north) };
}

orbit::State Flying::toOrbit(const Local &l) const
{
	const Place p = m_planet.place(m_planet.nearest(l.x, l.z), l.x, l.z);
	const orbit::Vec pos = toOrbitAxes(m_planet.sphere().point(p, l.altitude));
	return { pos, frame(p).out({ l.east, l.up, l.north }) };
}

Flying::Local Flying::fromOrbit(const orbit::State &s) const
{
	const Place p = m_planet.sphere().place(toSphereAxes(s.pos));
	const std::array<double, 2> at = m_planet.at(p);
	const orbit::Vec v = along(frame(p), s.vel);
	return { at[0], at[1], s.pos.length() - m_planet.sphere().radius(), v.x, v.y, v.z };
}

Flying::Step Flying::step(const Local &l, const orbit::Craft &craft, const std::array<double, 3> &thrust, double dt) const
{
	const Place from = m_planet.place(m_planet.nearest(l.x, l.z), l.x, l.z);
	const orbit::Frame before = frame(from);
	const orbit::State next = m_flight.step(toOrbit(l), craft, before.out({ thrust[0], thrust[1], thrust[2] }), dt);
	const Local where = fromOrbit(next);
	// the turn: the old grid's east, in the new grid (east toward north)
	const Place to = m_planet.place(m_planet.nearest(where.x, where.z), where.x, where.z);
	int turn = 0;
	if (to.face != from.face) {
		const orbit::Vec east = along(frame(to), before.east);
		turn = std::abs(east.x) > std::abs(east.z) ? (east.x > 0 ? 0 : 2) : (east.z > 0 ? 1 : 3);
	}
	return { where, turn };
}

} // namespace planet
