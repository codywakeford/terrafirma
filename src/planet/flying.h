// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "orbit/flight.h"
#include "planet/planet.h"
#include <array>

namespace planet {

/*
	Flight over a cube-sphere planet, in the map's own terms: a position in
	map nodes (fractional) and metres above the sea, a velocity in m/s along
	the grid where it is (east, up, north: the map's x, y, z there). Each step
	goes through the orbit model's planet-centred frame (orbit::Flight:
	gravity falling off with height, the air, the planet's turning), so
	climbing, coasting round and coming down are the planet's real physics,
	and crossing a face's edge is where the flight carries it.
*/
class Flying
{
public:
	struct Local
	{
		double x, z;      // map nodes
		double altitude;  // m above the sea
		double east, up, north; // m/s along the grid here
	};

	struct Step
	{
		Local where;
		int turn; // quarter turns the grid took under it (a face's edge crossed)
	};

	Flying(const Planet &planet, const orbit::Flight &flight) : m_planet(planet), m_flight(flight) {}

	orbit::State toOrbit(const Local &l) const;
	Local fromOrbit(const orbit::State &s) const;

	// `thrust`: N along the grid where it is (east, up, north).
	Step step(const Local &l, const orbit::Craft &craft, const std::array<double, 3> &thrust, double dt) const;

private:
	// the grid's directions at a place, in the orbit model's frame
	orbit::Frame frame(const terragen::Sphere::Place &p) const;

	const Planet &m_planet;
	const orbit::Flight &m_flight;
};

} // namespace planet
