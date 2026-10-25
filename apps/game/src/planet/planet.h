// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "terragen/sphere.h"
#include <array>
#include <cstdint>
#include <optional>
#include <string>

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
	wrong face's blocks. The net starts at the lowest node the map generator
	makes, and the largest planet (MAX_HALF) fills its width; what's north of
	the net is spare.

	Faces don't turn in the net: a face's grid is its square's, x east and z
	north as in terragen::Sphere.
*/
class Planet
{
public:
	static constexpr int32_t GAP = 16;     // nodes between faces: a map block
	// The map generator makes whole chunks only: with the default chunks (5
	// map blocks) from -30912 to 30927. The net starts at the lowest, and the
	// largest planet fills them across: three faces and two gaps, 61,808
	// nodes. (The server checks a world's chunks hold it: Server::init.)
	static constexpr int32_t ORIGIN = -30912;
	static constexpr int32_t MAX_HALF = 10296;

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

	// Where a node of `face`'s grid carried on past one of its edges is: the
	// node of the neighbour face there, exactly, and the turn between the
	// grids. Past an edge by k is a step across and k − 1 on (the faces meet
	// block for block). Past two edges at once, round a corner, there's
	// nothing (three faces meet there, not four): none; nor more than a face
	// past an edge. On the face, the
	// node itself. The engine reads a face's surroundings through this:
	// what's next to it, whatever the net puts there.
	std::optional<Step> halo(int face, int32_t x, int32_t z) const;

	// Something at (x, z) (fractional nodes) that has moved off `face` past
	// one edge: where it is on the neighbour face, exactly (its node is the
	// halo's, and where it is in that node turns with the grid), and the
	// turn. None if it's still on the face, round a corner, or further than
	// a face past the edge.
	struct Carried
	{
		double x, z;
		int face;
		int turn;
	};
	std::optional<Carried> carry(int face, double x, double z) const;
	// A vector along the ground turned with a grid (east toward north).
	static std::array<double, 2> turned(double dx, double dz, int turn);

	// A node of `face`'s grid carried on past its edges (x, z: map nodes,
	// fractional), put on the face the planet has it on: the map's node there.
	// Not exact a long way past an edge (one grid carried on isn't the next
	// face's), so for what's near, not for stepping: that's step().
	std::array<int32_t, 2> wrap(int face, double x, double z) const;

	// The column of an equirectangular map `size` blocks round (x east from
	// longitude 0, z north, the poles at ±size/2, as terragen's world model)
	// at a node's latitude and longitude: how a flat world's model is laid on
	// the planet. x wraps; z stays inside (1 − size/2 .. size/2).
	std::array<int32_t, 2> equirect(double x, double z, int32_t size) const;

	// How far a point is from the nearest of the 8 corners, where three faces
	// meet, along the ground (m on the sea): the landmarks stand there.
	double corner(double x, double z) const;

	// How far apart two points are, through the planet (m): map nodes
	// (fractional) on any faces, y m above the sea.
	double distance(double ax, double ay, double az, double bx, double by, double bz) const;

	// A direction turned by `turn` quarter turns, east toward north.
	static std::array<int, 2> turned(int dx, int dz, int turn);

	// GLSL for the sphere lens shader: planetAxes(face, out, east, north),
	// each face's axes from terragen::Sphere::axes.
	static std::string glslAxes();

private:
	terragen::Sphere m_sphere;
};

} // namespace planet

namespace planet {

/*
	The planet seen from an eye anywhere on it: where a point of the map
	(on any face) is from the eye, in a frame that's the eye face's grid at
	the eye (terragen::Sphere::Lens): x along the grid east, y up, z north,
	m. Next to the eye that's the blocks as the engine has them; over an
	edge the neighbour face carries on where it meets this one.

	The sphere lens shader (client/shaders/sphere/lens.glsl) is this, line
	for line, in floats (T = float); the tests hold the two forms to each
	other and to the planet.
*/
template <typename T>
class Lens
{
public:
	struct V3
	{
		T x, y, z;
	};

	// eye: map nodes (fractional), y m above the sea
	Lens(const Planet &planet, double x, double y, double z);

	// A point of the map: (x, z) nodes from the eye (the shader's scene is
	// the eye's neighbourhood), y m above the sea.
	V3 at(T east, T height, T north) const;

	int face; // the eye's
	typename terragen::Sphere::Lens<T> near;

private:
	T m_x, m_z; // the eye, map nodes
	T m_origin, m_pitch, m_half;
};

} // namespace planet
