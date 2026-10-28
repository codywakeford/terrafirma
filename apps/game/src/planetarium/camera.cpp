// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "planetarium/camera.h"
#include "constants.h"
#include <algorithm>
#include <cmath>

namespace planetarium {

static constexpr double LOWEST = 100;  // blocks over the sea
static constexpr double TURN = 0.004;  // radians a pixel, from a body radius up
static constexpr double STEP = 0.85;   // a wheel step's share of the altitude
static constexpr float CELL = 2;       // pixels a tile's cell may span before it splits

OrbitCamera::OrbitCamera(double sea, double farthest) : m_sea(sea), m_farthest(farthest), m_altitude(2.5 * sea)
{
}

void OrbitCamera::drag(double dx, double dy)
{
	const double k = TURN * std::clamp(m_altitude / m_sea, 0.002, 1.0);
	m_longitude -= dx * k;
	m_latitude = std::clamp(m_latitude + dy * k, -1.55, 1.55);
}

void OrbitCamera::zoom(double steps)
{
	m_altitude = std::clamp(m_altitude * std::pow(STEP, steps), LOWEST, m_farthest);
}

void OrbitCamera::focus(double latitude, double longitude, std::optional<double> altitude)
{
	m_latitude = std::clamp(latitude, -1.55, 1.55);
	m_longitude = longitude;
	if (altitude)
		m_altitude = std::clamp(*altitude, LOWEST, m_farthest);
}

OrbitCamera::Vec OrbitCamera::ray(double x, double y, double width, double height, v2f shift) const
{
	// the pixel as if the screen's middle were where the view's is
	x -= shift.X * width / 2;
	y += shift.Y * height / 2;
	// in the camera's own axes (left-handed: x right, y up, z ahead), then turned
	// into the planet's by the view's inverse
	const double t = std::tan(FOV / 2), aspect = width / std::max(1.0, height);
	core::vector3df d((float)((2 * x / width - 1) * t * aspect), (float)((1 - 2 * y / height) * t), 1);
	core::matrix4 inverse;
	view().getInverse(inverse);
	d = inverse.rotateAndScaleVect(d);
	d.normalize();
	return { d.X, d.Y, d.Z };
}

OrbitCamera::Vec OrbitCamera::up() const
{
	return { std::cos(m_latitude) * std::cos(m_longitude), std::sin(m_latitude),
		std::cos(m_latitude) * std::sin(m_longitude) };
}

OrbitCamera::Vec OrbitCamera::position() const
{
	const Vec u = up();
	const double d = m_sea + m_altitude;
	return { u.x * d, u.y * d, u.z * d };
}

far::Eye OrbitCamera::eyeAt(const Vec &p, double sea, float height)
{
	const double d = std::sqrt(p.x * p.x + p.y * p.y + p.z * p.z);
	const Vec u{ p.x / d, p.y / d, p.z / d };
	const double h = std::max(d - sea, 0.0);
	// what it sees of the body: the cap within the horizon's angle, as the
	// chord from the point under it (far::Tiles measures so); a little
	// over, for the land standing above the sea
	const double horizon = std::acos(sea / (sea + h));
	const float range = (float)(2 * sea * std::sin(std::min(horizon * 1.15, M_PI) / 2));
	// a tile of span s, seen from d, splits while d < s · split: its cells
	// (s / TILE) are wider than CELL pixels at d
	const float focal = height / (2 * std::tan(FOV / 2));
	const float split = focal / (far::TILE * CELL);
	return { { u.x * sea, u.y * sea, u.z * sea }, (float)h, 0, range, split };
}

OrbitCamera::Vec OrbitCamera::turn(const Vec &p, double spin)
{
	const double c = std::cos(spin), s = std::sin(spin);
	return { p.x * c - p.z * s, p.y, p.x * s + p.z * c };
}

core::matrix4 OrbitCamera::turned(const core::matrix4 &view, double spin)
{
	// turn() as a matrix (x' = x·M0 + y·M4 + z·M8, and so on), then the view
	const f32 c = (f32)std::cos(spin), s = (f32)std::sin(spin);
	core::matrix4 r;
	r[0] = c;
	r[8] = -s;
	r[2] = s;
	r[10] = c;
	return view * r;
}

core::matrix4 OrbitCamera::view() const
{
	const Vec u = up();
	// north along the ground under it: the screen's up
	const core::vector3df north((float)(-std::sin(m_latitude) * std::cos(m_longitude)), (float)std::cos(m_latitude),
			(float)(-std::sin(m_latitude) * std::sin(m_longitude)));
	core::matrix4 m;
	m.buildCameraLookAtMatrixLH(core::vector3df(0), core::vector3df((float)-u.x, (float)-u.y, (float)-u.z), north);
	return m;
}

core::matrix4 OrbitCamera::perspective(float aspect, float near, float far, v2f shift)
{
	core::matrix4 m;
	m.buildProjectionMatrixPerspectiveFovLH(FOV, aspect, near, far);
	// clip x and y gain shift · w (w is the depth): the drawing moves by shift
	m[8] += shift.X;
	m[9] += shift.Y;
	return m;
}

core::matrix4 OrbitCamera::projection(float aspect, v2f shift) const
{
	// near: well short of the highest ground under it; far: past the body's back
	const float near = (float)std::max(1.0, m_altitude * 0.2) * BS;
	const float far = (float)(m_altitude + 2.2 * m_sea) * BS;
	return perspective(aspect, near, far, shift);
}

} // namespace planetarium
