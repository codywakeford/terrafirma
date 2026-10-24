// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "terragen/sphere.h"
#include <array>
#include <cstdint>
#include <optional>

namespace planet {

/*
	The planet in the engine's map: the six faces of a cube-sphere
	(terragen::Sphere) laid out flat, as an unfolded net, in the engine's own
	coordinates. A position is still the engine's: faces are regions of the
	map, and every step from one face onto another goes through step().

	 z ↑  +---+ g +---+ g +---+
	      | 3 |   | 4 |   | 5 |
	      +---+   +---+   +---+
	        g       g       g
	      +---+ g +---+ g +---+
	      | 0 |   | 1 |   | 2 |
	      +---+   +---+   +---+  → x

	Between every two faces is a gap a map block wide (GAP): never a face's,
	filled with a seam node the game never lets anything through. Only some
	of the net's joins are real edges of the cube, so none are used as one:
	code that steps across a join without step() meets the seam, never the
	wrong face's blocks. The net starts at the map's lowest block, and the
	largest planet (MAX_HALF) fills the map's width exactly; what's north of
	the net is spare.

	Faces don't turn in the net: a face's grid is its square's, x east and z
	north as in terragen::Sphere.
*/
class Planet
{
public:
	static constexpr int32_t GAP = 16;     // nodes between faces: a map block
	static constexpr int32_t ORIGIN = -30992; // the map's lowest whole block (MAX_MAP_GENERATION_LIMIT)
	static constexpr int32_t MAX_HALF = 10328; // three faces and two gaps: the map's 62,000 nodes

	// A face's square: its lowest node, and its width (nodes).
	struct Square
	{
		int32_t x, z;
	};

	// One step across the map: where it lands, on which face, and how the
	// grid turned on the way (quarter turns, east toward north: 0 within a
	// face). A direction (dx, dz) taken off one face is turn(dx, dz) on the next.
	struct Step
	{
		int32_t x, z;
		int face;
		int turn;
	};

	explicit Planet(int32_t half);

	const terragen::Sphere &sphere() const { return m_sphere; }
	int32_t half() const { return m_sphere.half(); }
	int32_t width() const { return 2 * half(); }
	Square square(int face) const;

	// The face a node is on; none in a gap or off the net.
	std::optional<int> face(int32_t x, int32_t z) const;
	// The face nearest a point (fractional nodes): the gaps split down their
	// middles. For what's drawn, where a vertex on a face's edge is that face's.
	int nearest(double x, double z) const;

	// Where a point of a face is on the planet (fractional nodes).
	terragen::Sphere::Place place(int face, double x, double z) const;
	// And back: the map's point for a place (fractional nodes).
	std::array<double, 2> at(const terragen::Sphere::Place &p) const;

	// The node next to (x, z) on its face, `dx` or `dz` (one of them ±1)
	// along the face's grid: across an edge onto the neighbour face if need be.
	// (x, z) must be on a face.
	Step step(int32_t x, int32_t z, int dx, int dz) const;

	// A direction turned by `turn` quarter turns, east toward north.
	static std::array<int, 2> turned(int dx, int dz, int turn);

private:
	terragen::Sphere m_sphere;
};

} // namespace planet
