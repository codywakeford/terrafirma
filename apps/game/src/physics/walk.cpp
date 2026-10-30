// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "walk.h"

#include <algorithm>
#include <cmath>

namespace physics {

v3f walk(const Gait &gait, const Intent &intent, const Footing &now, float dt)
{
	// which way it's asked to go, in the world: forward is the yaw's +z
	v3f ask(intent.right, 0, intent.forward);
	if (ask.getLengthSQ() > 1)
		ask.normalize();
	const float s = std::sin(intent.yaw), c = std::cos(intent.yaw);
	// Luanti's yaw turns +z toward −x
	const v3f dir(ask.X * c - ask.Z * s, 0, ask.X * s + ask.Z * c);
	const v3f target = dir * (gait.walk * gait.run);

	// across, relative to what it stands on
	const v3f ground = now.on_ground ? now.ground_velocity : v3f(0);
	v3f across(now.velocity.X - ground.X, 0, now.velocity.Z - ground.Z);
	const v3f gap = target - across;
	float rate;
	if (!now.on_ground) {
		rate = gait.air;
	} else if (target.dotProduct(across) < 0 || target.getLength() < across.getLength()) {
		rate = gait.brake; // slowing, or turning back
	} else {
		// getting going, and turning, is quick (feet grip); only going
		// faster the same way past a walk, to a run, is slow
		const bool onward = target.dotProduct(across) > 0.95f * target.getLength() * across.getLength();
		rate = across.getLength() >= gait.walk && onward ? gait.push_on : gait.start;
	}
	const float most = rate * dt;
	across += gap.getLength() > most ? gap * (most / gap.getLength()) : gap;

	float up;
	if (now.on_ground)
		up = intent.jump ? ground.Y + std::sqrt(2 * gait.gravity * gait.jump) : ground.Y;
	else
		up = now.velocity.Y - gait.gravity * dt;
	return v3f(across.X + ground.X, up, across.Z + ground.Z);
}

} // namespace physics
