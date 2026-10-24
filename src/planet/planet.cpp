// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "planet.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

namespace planet {

using Place = terragen::Sphere::Place;

Planet::Planet(int32_t half) : m_sphere(half)
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
