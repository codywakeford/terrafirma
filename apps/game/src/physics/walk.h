// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "irr_v3d.h"

/*
	Walking (sprint 58): how a body moves under its own power. A pure rule:
	from what the player asks (which way, jump) and how the body is (its
	velocity, standing or not, what it stands on), the velocity it moves at
	next. physics::World's characters move at it.

	It has inertia. On the ground it gets up to walking speed in about a
	quarter of a second, and past it, to a run, more slowly; it brakes
	harder than it starts; changing direction at speed is a curve, not a
	snap. In the air it barely steers. It moves relative to what it stands
	on, so a cart carries it. A jump is a height, not a speed: the speed
	that height takes under this gravity.
*/
namespace physics {

struct Gait
{
	float walk = 4;          // nodes/s
	float run = 1;           // × walk at most when running (sprint: the caller's)
	float start = 18;        // nodes/s² up to walking speed
	float push_on = 5;       // nodes/s² from walking up to running
	float brake = 28;        // nodes/s² slowing (letting go, or turning back)
	float air = 3;           // nodes/s² steering in the air
	float jump = 1.25f;      // nodes high
	float gravity = 9.81f;   // nodes/s²
};

struct Intent
{
	float forward = 0, right = 0; // −1..1 each (a stick's: it's capped at 1 together)
	float yaw = 0;                // radians: which way "forward" is (Luanti's: 0 faces +z)
	bool jump = false;
};

struct Footing
{
	v3f velocity;
	bool on_ground = false;
	v3f ground_velocity{ 0 };
};

// Its velocity after `dt` more of walking.
v3f walk(const Gait &gait, const Intent &intent, const Footing &now, float dt);

} // namespace physics
