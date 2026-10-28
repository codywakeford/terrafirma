// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "far/tiling.h"
#include "far/tiles.h"
#include <matrix4.h>

namespace planetarium {

/*
	A camera in orbit round a body, looking down at its centre: where it is
	over the body (latitude, longitude) and how high above its sea. Dragging
	moves it round, slower the lower it is (the ground under it moves at
	about the same speed on screen); zooming moves it up and down, from a
	few body radii out to 100 blocks over the sea. In doubles; what it gives
	the GPU is relative to itself, in floats.
*/
class OrbitCamera
{
public:
	using Vec = far::Tiling::Vec;

	explicit OrbitCamera(double sea); // the body's sea: blocks from its centre

	// pixels the mouse moved while held; wheel steps (up: closer)
	void drag(double dx, double dy);
	void zoom(double steps);

	double altitude() const { return m_altitude; }
	// where it is, from the body's centre (blocks)
	Vec position() const;
	// what far::Tiles chooses for, on a screen `height` pixels high: the
	// point under it at the sea; out to what it can see of the body (its
	// horizon); tiles fine enough that a cell is about two pixels across
	far::Eye eye(float height) const;

	// view and projection, the camera at the origin (camera-relative: a
	// point p is drawn at (p − position()) · BS)
	core::matrix4 view() const;
	core::matrix4 projection(float aspect) const;

private:
	Vec up() const; // unit, from the centre to the camera

	const double m_sea;
	double m_latitude = 0.4, m_longitude = 0.3; // radians
	double m_altitude;
};

} // namespace planetarium
