// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "director.h"
#include "ui.h"
#include "client/client.h"
#include "constants.h"
#include "exceptions.h"
#include "porting.h"
#include <cmath>

namespace shell {

static const float GLIDE_SECONDS = 1.2f;

static float number(const Json::Value &v, const char *field)
{
	if (!v[field].isNumeric())
		throw BaseException(std::string("shell: camera orbit needs a number '") + field + "'");
	return v[field].asFloat();
}

bool Director::Orbit::operator!=(const Orbit &o) const
{
	return center != o.center || radius != o.radius || height != o.height || period != o.period;
}

Director::Orbit Director::readOrbit(const Json::Value &value)
{
	const Json::Value &orbit = value["orbit"];
	if (!orbit.isObject() || !orbit["center"].isObject())
		throw BaseException("shell: the camera value needs orbit = { center, radius, height, period }");
	const Json::Value &c = orbit["center"];
	Orbit o{ v3f(number(c, "x"), number(c, "y"), number(c, "z")),
			number(orbit, "radius"), number(orbit, "height"), number(orbit, "period") };
	if (o.radius <= 0 || o.period <= 0)
		throw BaseException("shell: camera orbit radius and period must be positive");
	return o;
}

Camera::Shot Director::along(const Orbit &o) const
{
	v3f offset(std::cos(m_angle) * o.radius, o.height, std::sin(m_angle) * o.radius);
	return { o.center + offset, o.center };
}

static v3f mix(v3f a, v3f b, float t)
{
	return a + (b - a) * t;
}

void Director::update()
{
	u64 now = porting::getTimeMs();
	float dtime = m_last_ms ? (now - m_last_ms) / 1000.0f : 0.0f;
	m_last_ms = now;

	Camera *camera = m_opts.client->getCamera();
	const Json::Value &value = m_opts.ui.get("camera");
	if (value.isNull()) {
		if (m_orbit) {
			m_orbit.reset();
			camera->setShot(std::nullopt);
		}
		return;
	}

	Orbit orbit = readOrbit(value);
	if (!m_orbit || *m_orbit != orbit) {
		// glide from what the camera shows now
		v3f eye = camera->getPosition() / BS;
		m_from = { eye, eye + camera->getDirection() * 10.0f };
		m_glide = 0.0f;
		m_orbit = orbit;
	}

	m_angle += dtime * 2.0f * (float)M_PI / orbit.period;
	Camera::Shot want = along(orbit);
	if (m_glide < 1.0f) {
		m_glide = std::fmin(1.0f, m_glide + dtime / GLIDE_SECONDS);
		float t = m_glide * m_glide * (3.0f - 2.0f * m_glide);
		want = { mix(m_from.position, want.position, t), mix(m_from.target, want.target, t) };
	}
	camera->setShot(want);
}

} // namespace shell
