// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "irr_v3d.h"
#include "irrlichttypes.h"
#include "planet/atlas.h"
#include <matrix4.h>
#include <array>
#include <optional>
#include <unordered_map>

/*
	An atlas planet as the eye sees it (planning/sprint-30-atlas.md, phase
	4). The eye, the player and what's round them stay in the eye's grid;
	every map block is drawn with the map from its own region's grid into
	the eye's carried on (planet::Atlas::view), so blocks across a seam land
	where they are, and far ones curve away with the planet.

	A block's mesh is already on the planet (its vertices bent by its grid's
	chart, round its middle: Atlas::blockChart, MapBlockMesh), so every
	block is drawn with the eye's one map, moved to the block's middle:
	neighbours meet exactly, in a grid and over a seam. A block's middle is
	kept (it never changes); the eye's frame is taken once a frame (update),
	and each drawn block's transform from the two, kept for the frame.
*/
class AtlasView
{
public:
	explicit AtlasView(const planet::Atlas &atlas) : m_atlas(atlas) {}

	// The eye this frame: where the camera and the player's feet are (BS
	// units) and the camera offset the scene is drawn from (nodes).
	void update(v3f camera, v3f feet, v3s16 offset);

	// How to draw a block: its mesh's coordinates (BS, on the planet from
	// the block's middle) to the scene's. None if the eye or the block has
	// no cell (seam).
	const core::matrix4 *transform(v3s16 block);
	// How to draw anything on the planet given from its middle (a mesh's
	// coordinates: BS, from `centre`, a point on the planet): the far view's
	// tiles. None while the eye has no grid.
	std::optional<core::matrix4> place(const terragen::Atlas::Vec &centre) const;
	// The eye this frame: its point on the planet, and where that is in the
	// scene (BS units, as the camera is drawn). None while it has no grid.
	const std::optional<planet::Atlas::Frame> &eye() const { return m_eye; }

private:
	const std::optional<planet::Atlas::BlockChart> &chart(v3s16 block);

	const planet::Atlas &m_atlas;
	std::optional<planet::Atlas::Frame> m_eye;
	std::array<terragen::Atlas::Vec, 3> m_inverse; // the eye's axes inverted: rows
	v3f m_camera{ 1e30f, 0, 0 };
	v3f m_feet;
	v3s16 m_offset;
	std::unordered_map<v3s16, std::optional<planet::Atlas::BlockChart>> m_blocks; // for good
	std::unordered_map<v3s16, std::optional<core::matrix4>> m_frame;        // this frame's
};
