// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "map.h"
#include <algorithm>
#include <cmath>

namespace terragen {

// latitude = π·z/size; longitude 0 at the map's west edge (x = −size/2)
double Map::latitudeAt(double z) const
{
	return M_PI * z / m_size;
}

double Map::longitudeAt(double x) const
{
	double turns = (x + m_size / 2) / m_size;
	return 2 * M_PI * (turns - std::floor(turns));
}

double Map::zAt(double latitude) const
{
	return latitude * m_size / M_PI;
}

double Map::xAt(double longitude) const
{
	double turns = longitude / (2 * M_PI);
	return (turns - std::floor(turns)) * m_size - m_size / 2;
}

double Map::grown(double altitude) const
{
	return std::clamp(std::log(std::max(altitude, SPREAD) / SPREAD) / std::log(FULL / SPREAD), 0.0, 1.0);
}

double Map::share(double altitude) const
{
	double block = 2 * M_PI * m_radius / m_size; // a block across at the equator, m
	return (std::pow(block, grown(altitude)) - 1) / (block - 1);
}

double Map::curve(double altitude) const
{
	double ground = CURVE_PER_SIZE * m_size, s = grown(altitude);
	return std::pow(ground, 1 - s) * std::pow(m_radius, s);
}

Map::Across Map::across(double altitude, double latitude) const
{
	double t = share(altitude), block = M_PI * m_radius / m_size;
	return { 1 + t * (2 * block * std::cos(latitude) - 1), 1 + t * (block - 1) };
}

} // namespace terragen
