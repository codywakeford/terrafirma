// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <cstdint>

namespace terragen {

/*
	The blocks as a map of the planet: `size` blocks around the equator,
	size/2 from pole to pole (equirectangular: x is longitude, z latitude,
	+z north, the poles at z = ±size/2). Cheap: no world model behind it, so
	the client's far view has one too.

	Up the air, the map becomes the planet. Near the ground a block is a
	metre every way and the land curves gently (a sphere CURVE_PER_SIZE
	sizes round, so the horizon falls inside the far view); by FULL a block
	across is the planet it maps (2πR/size east at the equator, πR/size
	north) and the curve is the planet's. In between both grow
	geometrically with height, a block's span a little slower than the
	height itself, so the land below shrinks as you climb, if more slowly
	than over a real planet (it has 490 times as far to go). The far view
	draws the land by it and flight over the blocks moves by it, so they
	agree.
*/
class Map
{
public:
	static constexpr double BOUNDARY = 30000; // m above the sea: space
	static constexpr double SPREAD = 400;     // m above the sea, where it starts (the land tops out below)
	static constexpr double FULL = 400000;    // m above the sea, where it's the planet
	static constexpr double CURVE_PER_SIZE = 8; // the ground's curve, in map sizes

	struct Across
	{
		double east, north; // metres of planet per block
	};

	Map(int32_t size, double radius) : m_size(size), m_radius(radius) {}

	int32_t size() const { return m_size; }
	double radius() const { return m_radius; } // m

	// Map coordinates ↔ the planet: radians north and east. Fractional both
	// ways: a block across is hundreds of metres of planet.
	double latitudeAt(double z) const;
	double longitudeAt(double x) const;
	double zAt(double latitude) const;
	double xAt(double longitude) const;

	// How far the map has become the planet: 0 at SPREAD and below .. 1 at
	// FULL and above, the share of the way a block's span has grown.
	double share(double altitude) const;
	Across across(double altitude, double latitude) const;
	// The radius the land curves with, m.
	double curve(double altitude) const;

private:
	double grown(double altitude) const; // 0 .. 1, geometrically with height

	int32_t m_size;
	double m_radius;
};

} // namespace terragen
