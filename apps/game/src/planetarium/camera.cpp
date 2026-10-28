// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "planetarium/camera.h"
#include "constants.h"
#include <algorithm>
#include <cmath>

namespace planetarium {

static constexpr double LOWEST = 100;  // blocks over the sea
static constexpr double FARTHEST = 6;  // body radii over the sea
static constexpr double TURN = 0.004;  // radians a pixel, from a body radius up
static constexpr double STEP = 0.85;   // a wheel step's share of the altitude
static constexpr float FOV = 1.0f;     // radians, vertical
static constexpr float CELL = 2;       // pixels a tile's cell may span before it splits

OrbitCamera::OrbitCamera(double sea) : m_sea(sea), m_altitude(2.5 * sea)
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
	m_altitude = std::clamp(m_altitude * std::pow(STEP, steps), LOWEST, FARTHEST * m_sea);
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

far::Eye OrbitCamera::eye(float height) const
{
	const Vec u = up();
	const double h = m_altitude;
	// what it sees of the body: the cap within the horizon's angle, as the
	// chord from the point under it (far::Tiles measures so); a little
	// over, for the land standing above the sea
	const double horizon = std::acos(m_sea / (m_sea + h));
	const float range = (float)(2 * m_sea * std::sin(std::min(horizon * 1.15, M_PI) / 2));
	// a tile of span s, seen from d, splits while d < s · split: its cells
	// (s / TILE) are wider than CELL pixels at d
	const float focal = height / (2 * std::tan(FOV / 2));
	const float split = focal / (far::TILE * CELL);
	return { { u.x * m_sea, u.y * m_sea, u.z * m_sea }, (float)h, 0, range, split };
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

core::matrix4 OrbitCamera::projection(float aspect) const
{
	// near: well short of the highest ground under it; far: past the body's back
	const float near = (float)std::max(1.0, m_altitude * 0.2) * BS;
	const float far = (float)(m_altitude + 2.2 * m_sea) * BS;
	core::matrix4 m;
	m.buildProjectionMatrixPerspectiveFovLH(FOV, aspect, near, far);
	return m;
}

} // namespace planetarium
