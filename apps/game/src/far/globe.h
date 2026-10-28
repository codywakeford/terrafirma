// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "irrlichttypes.h"
#include "terragen/atlas.h"
#include <array>

namespace far {

struct TileKey;

/*
	The far view's tiling of a planet as a sphere: a cube's six faces, each
	a quadtree, bent onto the sphere (equal angles, so cells are near
	squares everywhere). Only where the far view samples the land: it knows
	nothing of how the planet's blocks are stored, so the same tiles serve
	the ground to orbit and beyond.

	A tile's key is its face, its level (0 finest: cells about a block
	wide), and its place on the face at that level; at the top level one
	tile is a whole face. Within a tile, as on the map, positions are in
	finest cells: x and z along the face, from the tile's corner, a cell
	centred on whole numbers.
*/
class Globe
{
public:
	using Vec = terragen::Atlas::Vec;

	explicit Globe(double radius);

	double radius() const { return m_radius; }
	// the level whose one tile is a whole face
	s32 top() const { return m_top; }
	// finest cells along a face's edge
	s32 cells() const { return m_cells; }

	// Which way (unit, from the centre) a point of a face is, at (fx, fz)
	// finest cells from its corner; its latitude and longitude (radians, as
	// terragen::Atlas: y up, longitude from +x toward +z).
	Vec direction(int face, double fx, double fz) const;
	static std::array<double, 2> place(const Vec &direction);

	// A position in a tile (x, z in finest cells from its corner, as the far
	// mesh has them; y the height, from the sea's radius, in blocks) on the
	// planet, from the planet's centre.
	Vec point(const TileKey &key, double x, double y, double z) const;
	// A tile's middle at the sea: where its mesh is drawn from.
	Vec middle(const TileKey &key) const;
	// How far across a tile is (blocks, along its diagonal at the sea).
	double diagonal(const TileKey &key) const;

private:
	double m_radius;
	s32 m_top, m_cells;
};

} // namespace far
