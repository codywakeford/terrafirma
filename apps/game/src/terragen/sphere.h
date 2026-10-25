// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <array>
#include <cstdint>

namespace terragen {

/*
	The planet as blocks: a cube blown up into a ball (an equiangular
	cube-sphere). Each of the six faces is a square grid of blocks, `half`
	blocks from its centre to its edge each way, so a face fits in the
	engine's coordinates (|x|, |z| < 31000). Every column of blocks points at
	the planet's centre: up is always up, and gravity always pulls down a
	column.

	- Along a face's centre lines a block is exactly a metre across; away
	  from them it shrinks a little (cellSides gives the range).
	- Two faces meet along an edge block for block: past an edge the grid
	  turns, nothing else. place() of a point past an edge finds it on the
	  neighbour face.
	- The 8 corners are where three faces meet: the only places a block has
	  three neighbours round a point, not four. They're the planet's
	  landmarks, not its bugs.

	Faces 0–3 go round the equator east, 4 is the north pole and 5 the
	south. A face's grid has x east and z north, as the engine's map does;
	on the polar faces "east" and "north" are the grid's directions, not the
	compass's.
*/
class Sphere
{
public:
	struct Vec
	{
		double x, y, z;
	};

	// Where on the planet: a face, and blocks east and north of its centre
	// (fractional).
	struct Place
	{
		int face;
		double x, z;
	};

	// The grid's directions at a place, in the planet's frame: unit vectors.
	// East and north follow the grid (they're square only on the centre lines).
	struct Frame
	{
		Vec east, up, north;
	};

	// A face's axes in the planet's frame (x to longitude 0 on the equator,
	// y to the north pole, z to 90° east): out of the face, and its grid's
	// east and north.
	struct Axes
	{
		Vec out, east, north;
	};
	static const Axes &axes(int face);

	explicit Sphere(int32_t half);

	int32_t half() const { return m_half; }
	double radius() const { return m_radius; } // m: a block is a metre along the centre lines

	Vec direction(const Place &p) const; // unit, from the centre
	Vec point(const Place &p, double height) const; // m from the centre
	Place place(const Vec &direction) const; // any non-zero vector: where it points
	Frame frame(const Place &p) const;
	double latitude(const Place &p) const;  // radians north
	double longitude(const Place &p) const; // radians east

	// A place past its face's edge, put on the face it's on.
	Place settle(const Place &p) const { return place(direction(p)); }

	// The smallest and largest side of a block on the sea, m.
	std::array<double, 2> cellSides() const;

	/*
		The planet seen from an eye: where points near it are, in a frame
		that is the grid itself at the eye, so the near world is exactly the
		blocks as the engine has them and everything further off curves away
		as it does on the planet. Points are given as blocks from the eye
		(east, north, on its face) and m above the sea; the answer is m from
		the eye, x along the grid east, y up, z along the grid north.

		Written to keep its precision in floats a whole planet across: the
		far view and the near world's shaders do the same sums (T = float),
		and the tests hold them to it.
	*/
	template <typename T>
	class Lens
	{
	public:
		struct V3
		{
			T x, y, z;
		};

		Lens(const Sphere &s, const Place &eye, double height);

		V3 at(T east, T north, T height) const;

		// What a shader needs to do the same: the eye's angles and height,
		// the radius, radians per block, and the frame's inverse (rows, in
		// the face's own axes: east, out, north).
		T angleEast, angleNorth, eyeHeight, radius, step;
		std::array<V3, 3> rows;
	};

private:
	double angle(double blocks) const; // blocks from the centre → radians

	int32_t m_half;
	double m_radius;
};

} // namespace terragen
