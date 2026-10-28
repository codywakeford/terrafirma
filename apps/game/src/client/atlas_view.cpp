// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "atlas_view.h"
#include "constants.h" // BS

void AtlasView::update(v3f camera, v3f feet, v3s16 offset)
{
	if (camera == m_camera && feet == m_feet && offset == m_offset)
		return;
	m_camera = camera;
	m_feet = feet;
	m_offset = offset;
	m_frame.clear();
	// the camera's node, or (the camera in a gap between shells, or a seam)
	// the feet's: the player's grid carries on to the camera
	const v3f at = camera / BS, foot = feet / BS;
	const planet::Atlas::Node eye = planet::Atlas::moverNode(at.X, at.Y, at.Z);
	const planet::Atlas::Node stand = planet::Atlas::moverNode(foot.X, foot.Y, foot.Z);
	const planet::Atlas::Node grid = m_atlas.cell(eye) ? eye : stand;
	m_eye = m_atlas.frame(grid, { at.X, at.Y, at.Z });
	if (!m_eye)
		return;
	// the eye's axes inverted (rows of the inverse, by cofactors)
	using V = terragen::Atlas::Vec;
	const auto cross = [](V a, V b) { return V{ a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x }; };
	const std::array<V, 3> &j = m_eye->axes;
	const V r0 = cross(j[1], j[2]), r1 = cross(j[2], j[0]), r2 = cross(j[0], j[1]);
	const double det = j[0].x * r0.x + j[0].y * r0.y + j[0].z * r0.z;
	m_inverse = { V{ r0.x / det, r0.y / det, r0.z / det }, V{ r1.x / det, r1.y / det, r1.z / det },
		V{ r2.x / det, r2.y / det, r2.z / det } };
}

const std::optional<planet::Atlas::BlockChart> &AtlasView::chart(v3s16 block)
{
	const auto known = m_blocks.find(block);
	if (known != m_blocks.end())
		return known->second;
	return m_blocks.emplace(block, m_atlas.blockChart({ block.X, block.Y, block.Z })).first->second;
}

const core::matrix4 *AtlasView::transform(v3s16 block)
{
	auto [it, fresh] = m_frame.try_emplace(block);
	if (fresh && m_eye)
		if (const auto &c = chart(block))
			it->second = place(c->centre);
	return it->second ? &*it->second : nullptr;
}

std::optional<core::matrix4> AtlasView::place(const terragen::Atlas::Vec &centre) const
{
	if (!m_eye)
		return std::nullopt;
	// a vertex v (BS, on the planet from C) is the planet's C + v/BS; seen
	// from the eye, in its grid: eye + inverse·(C + v/BS − eye's point);
	// drawn at BS·(that − offset)
	using V = terragen::Atlas::Vec;
	const V d{ centre.x - m_eye->origin.x, centre.y - m_eye->origin.y, centre.z - m_eye->origin.z };
	core::matrix4 w;
	for (int r = 0; r < 3; r++) {
		const V &row = m_inverse[r];
		w[r] = (f32)row.x;
		w[4 + r] = (f32)row.y;
		w[8 + r] = (f32)row.z;
		const double at = m_eye->at[r] + row.x * d.x + row.y * d.y + row.z * d.z;
		w[12 + r] = (f32)((at - (r == 0 ? m_offset.X : r == 1 ? m_offset.Y : m_offset.Z)) * BS);
	}
	return w;
}
