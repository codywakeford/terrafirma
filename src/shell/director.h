// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "client/camera.h"
#include <json/json.h>
#include <optional>

class Client;

namespace shell {

class Ui;

/*
	Points the camera where game Lua asks (the UI value "camera"), smoothly,
	on the client: a server-driven camera would step at the server's rate.

	  camera = { orbit = { center = {x,y,z}, radius, height, period } }

	circles `center` at `radius`, `height` above it, once per `period`
	seconds, looking at it. A new orbit glides there from wherever the camera
	is; nil hands the camera back to the player.
*/
class Director
{
public:
	struct Opts {
		const Ui &ui;
		Client *client; // its camera exists once the game draws
	};

	explicit Director(const Opts &opts) : m_opts(opts) {}

	// Once per frame (keeps its own clock). Throws on a malformed "camera"
	// value: that's a bug in the Lua that sent it.
	void update();

private:
	struct Orbit
	{
		v3f center;
		float radius, height, period;
		bool operator!=(const Orbit &o) const;
	};

	static Orbit readOrbit(const Json::Value &value);
	Camera::Shot along(const Orbit &orbit) const;

	Opts m_opts;
	std::optional<Orbit> m_orbit;
	float m_angle = -1.5708f; // start south of the centre, looking north
	Camera::Shot m_from{};    // where a glide starts
	float m_glide = 1.0f;     // 0 → 1 over a glide
	u64 m_last_ms = 0;
};

} // namespace shell
