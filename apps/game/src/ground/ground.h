// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "irr_v3d.h"
#include "mapnode.h"
#include "nodedef.h"
#include <array>
#include <cmath>
#include <optional>

/*
	Ground: the shape natural loose ground takes. Undisturbed loose ground
	(dirt, sand, gravel, snow, clay: the `loose` group) is a height field
	to a sixteenth of a block, drawn and stood on in voxels of a sixteenth. Nothing is stored: a loose block
	with nothing walkable over it takes its shape from the columns round
	it, every time it's asked.

	The field is at column corners, where four columns meet. A corner's
	height, for one column, is the average of the surfaces of the columns
	there that are within a block of each other (its cluster): a step of
	one block is a ramp, a drop of two a cliff. It's never lowered under
	the column's own surface where a column there is higher, or solid at
	the same height: a face hidden against a full block stays hidden. A
	function of the four columns alone, so every column at a corner, in
	any map block, agrees.

	A column's top block is only lowered within its own cell; where the
	ground rises it fills into the air cell above it (up to a block).
*/
namespace ground {

constexpr int STEPS = 16; // a block's sixteenths

enum class Kind : uint8_t {
	None,  // nothing walkable near: open air, water
	Loose, // grains: takes the field's shape
	Solid, // keeps its block's shape
};

// One column at a corner: what its surface is, and its level (the block
// under it is at top - 1).
struct Column
{
	Kind kind;
	int top;
};

// A corner's height, in sixteenths of the world (a level × STEPS), for the
// column `own` of the four there.
int corner(const std::array<Column, 4> &around, size_t own);

/*
	A smoothed block's top, in voxels: STEPS × STEPS columns of a sixteenth,
	each standing to a sixteenth (0..2·STEPS above the block's base: it may
	rise into the cell above). Each is the field at its middle: the plane
	through the block's corners (two triangles, split from corner 0 to 3),
	to the nearest sixteenth.

	Corners: 0 (−x −z), 1 (+x −z), 2 (−x +z), 3 (+x +z). Sides: 0 −x
	(corners 0, 2), 1 +x (1, 3), 2 −z (0, 1), 3 +z (2, 3): where the
	neighbour's field stands at each of the side's two corners, in that order
	(the block's own sixteenths). A neighbour's voxels along the side stand
	no lower than the line between them less `EDGE` (the field rises at most
	two blocks across one: a sixteenth over half a voxel, half a sixteenth
	rounding): a wall from there up to ours, where ours is higher, meets it.
*/
struct Top
{
	std::array<int, 4> corners;
	std::array<std::array<int, 2>, 4> sides;
	std::array<std::array<u8, STEPS>, STEPS> cells; // [x][z]

	// The voxel column's height under (x, z) within the block (0..1 each),
	// in blocks: what's drawn and stood on.
	float height(float x, float z) const;
	// The field (the plane through the corners) at (x, z), in sixteenths.
	float field(float x, float z) const;
	// Where the neighbour across side s stands no lower than, at voxel k
	// along it (from the side's first corner), in sixteenths.
	int beside(int s, int k) const;
};

constexpr int EDGE = 2;

constexpr int SIDE_CORNERS[4][2] = { { 0, 2 }, { 1, 3 }, { 0, 1 }, { 2, 3 } };
constexpr int SIDE_DIR[4][2] = { { -1, 0 }, { 1, 0 }, { 0, -1 }, { 0, 1 } };

// How far a column's surface is looked for, up and down: a cluster of four
// columns spans at most 3, so anything further can't change a corner.
constexpr int REACH = 3;

class Ground
{
public:
	explicit Ground(const NodeDefManager &ndef) : m_ndef(ndef) {}

	Kind kind(const MapNode &n) const;

	/*
		The top of the block at p if it's smoothed: loose, with nothing
		walkable over it. `get(v3s16) → MapNode` reads the world (a voxel
		manipulator, a map).
	*/
	template <class Get>
	std::optional<Top> top(const Get &get, v3s16 p) const;

private:
	// The column at (x, z)'s surface nearest to level `near` (ties: the
	// higher), within REACH; past it, a wall (ground all the way up) or
	// nothing.
	template <class Get>
	Column column(const Get &get, s16 x, s16 z, int near) const;

	bool walkable(const MapNode &n) const { return m_ndef.get(n).walkable; }

	const NodeDefManager &m_ndef;
};

template <class Get>
Column Ground::column(const Get &get, s16 x, s16 z, int near) const
{
	for (int d = 0; d <= REACH; d++) {
		for (int top : { near + d, near - d }) {
			if (walkable(get(v3s16(x, top - 1, z))) && !walkable(get(v3s16(x, top, z))))
				return { kind(get(v3s16(x, top - 1, z))), top };
			if (d == 0)
				break;
		}
	}
	if (walkable(get(v3s16(x, near + REACH, z))))
		return { Kind::Solid, near + REACH + 1 };
	return { Kind::None, near - REACH - 1 };
}

template <class Get>
std::optional<Top> Ground::top(const Get &get, v3s16 p) const
{
	if (kind(get(p)) != Kind::Loose || walkable(get(p + v3s16(0, 1, 0))))
		return std::nullopt;
	const int own = p.Y + 1;
	const int base = p.Y * STEPS;

	// the 4 × 4 columns round the block's corners, (−1..2) each way
	std::array<std::array<Column, 4>, 4> cols;
	for (int a = 0; a < 4; a++)
		for (int b = 0; b < 4; b++)
			cols[a][b] = (a == 1 && b == 1) ? Column{ Kind::Loose, own }
					: column(get, p.X - 1 + a, p.Z - 1 + b, own);

	// the height, for column (a, b) of the grid, of its corner (cx, cz)
	auto at = [&](int a, int b, int cx, int cz) {
		const Column &c = cols[a][b];
		if (c.kind != Kind::Loose)
			return c.top * STEPS;
		int x0 = a + cx - 1, z0 = b + cz - 1; // the corner's columns
		std::array<Column, 4> around = {
			cols[x0][z0], cols[x0 + 1][z0], cols[x0][z0 + 1], cols[x0 + 1][z0 + 1] };
		return corner(around, (1 - cx) + 2 * (1 - cz));
	};

	Top t;
	for (int i = 0; i < 4; i++)
		t.corners[i] = at(1, 1, i & 1, i >> 1) - base;
	for (int x = 0; x < STEPS; x++)
		for (int z = 0; z < STEPS; z++)
			t.cells[x][z] = u8(std::lround(t.field((x + 0.5f) / STEPS, (z + 0.5f) / STEPS)));
	for (int s = 0; s < 4; s++) {
		int a = 1 + SIDE_DIR[s][0], b = 1 + SIDE_DIR[s][1];
		for (int k = 0; k < 2; k++) {
			int i = SIDE_CORNERS[s][k];
			int cx = i & 1, cz = i >> 1;
			// the same world corner, seen from the neighbour; one two or
			// more lower is past where its own corners can be read from
			// here (its cluster reaches beyond REACH): its lowest, so the
			// wall reaches into its ground rather than short of it
			const Column &n = cols[a][b];
			t.sides[s][k] = (n.kind == Kind::Loose && own - n.top >= 2)
					? (n.top - 1) * STEPS - base
					: at(a, b, cx - SIDE_DIR[s][0], cz - SIDE_DIR[s][1]) - base;
		}
	}
	return t;
}

} // namespace ground
