// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "far/tiling.h"
#include "far/tiles.h"
#include <matrix4.h>
#include <optional>

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

	// the body's sea: blocks from its centre; the farthest it goes over it
	OrbitCamera(double sea, double farthest);
	static constexpr float FOV = 1.0f; // radians, vertical

	// pixels the mouse moved while held; wheel steps (up: closer)
	void drag(double dx, double dy);
	void zoom(double steps);
	// over a place (radians), at an altitude (blocks over the sea; kept
	// within the camera's range), or at its own
	void focus(double latitude, double longitude, std::optional<double> altitude = std::nullopt);

	double altitude() const { return m_altitude; }
	double latitude() const { return m_latitude; }
	double longitude() const { return m_longitude; }
	// where it is, from the body's centre (blocks)
	Vec position() const;
	// what far::Tiles chooses for, on a screen `height` pixels high (eyeAt,
	// from where it is)
	far::Eye eye(float height) const { return eyeAt(position(), m_sea, height); }
	// What far::Tiles chooses for from a point `p` (blocks from a body's
	// centre, its sea `sea`): the point under it at the sea; out to what it
	// can see of the body (its horizon); tiles fine enough that a cell is
	// about two pixels across on a screen `height` pixels high.
	static far::Eye eyeAt(const Vec &p, double sea, float height);

	// view and projection, the camera at the origin (camera-relative: a
	// point p is drawn at (p − position()) · BS)
	core::matrix4 view() const;
	core::matrix4 projection(float aspect, v2f shift = v2f()) const;
	// A perspective, its middle moved `shift` (the screen's half widths and
	// heights, +x right, +y up): what's ahead is drawn there, not at the
	// screen's middle (the free part of a screen half under a panel).
	static core::matrix4 perspective(float aspect, float near, float far, v2f shift);

	// A body that spins: a point of its own frame (its ground's) where it
	// is in space, the body turned by `spin` radians about its axis (y),
	// eastward (longitude grows by it); and back.
	static Vec turn(const Vec &p, double spin);
	// A view that draws a spun body's own frame where the body is: what's
	// given in its frame turned (turn), then seen by `view`.
	static core::matrix4 turned(const core::matrix4 &view, double spin);

	// The way from the camera through a pixel (unit, planet frame), on a
	// screen `width` × `height`.
	Vec ray(double x, double y, double width, double height, v2f shift = v2f()) const;

private:
	Vec up() const; // unit, from the centre to the camera

	double m_sea, m_farthest; // (not const: a camera is replaced when it goes to another body)
	double m_latitude = 0.4, m_longitude = 0.3; // radians
	double m_altitude;
};

} // namespace planetarium
