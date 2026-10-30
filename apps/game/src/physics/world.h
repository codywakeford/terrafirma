// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "irr_aabb3d.h"
#include "irr_v3d.h"
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <vector>

/*
	Physics (sprint 57): rigid bodies on Jolt Physics. A pure module: it
	knows boxes, densities, hinges and ground, never a machine, a node or
	an entity (the workshop compiles those into it).

	Units are the world's nodes (a node is a metre), the world's frame
	(node centres at whole numbers), seconds and kilograms. Positions are
	doubles: Jolt is built in double precision.

	An assembly is what a machine becomes: bodies (each a rigid group of
	parts) and hinges between them, all given in the assembly's own frame,
	whose origin and turn are its pose. Its bodies never collide with each
	other. It's Live (simulated) or Frozen (kept as a pose, out of Jolt):
	the tier (planning/simulation-scales.md). Held, it's kinematic: moved
	where its holder says, pushing what it meets.

	Characters (sprint 58) are Jolt's virtual characters: capsules that walk
	by collide-and-slide, up steps, over slopes, standing on (and carried by)
	what moves, pushing bodies as hard as they're strong. A character moves
	at the velocity it's given (its controller's: physics/walk.h), or is
	placed (a player whose client moves them: it pushes on its way there).
	Carried, an assembly is pulled toward where it's to be as hard as its
	carrier can: light things follow, heavy ones lag or drag.

	Ground is asked of the caller a map block (16³ nodes) at a time, and
	only round Live assemblies and characters.

	Deterministic: one thread, a fixed step; the same calls give the same
	bodies, bit for bit.
*/
namespace physics {

struct PhysicsError : std::runtime_error
{
	using std::runtime_error::runtime_error;
};

constexpr int BLOCK = 16;     // nodes a side of a block of ground
constexpr double STEP = 1.0 / 60;

struct Box
{
	aabb3f box;     // nodes, its body's frame
	float density;  // kg/m³
	float friction; // Coulomb, 0..1+
};

// A smooth cylinder a body collides as instead of its boxes (a wheel: a
// round part on a hinge rolls true, not lumpy). Its mass is still its
// boxes'.
struct Round
{
	v3f centre, axis; // axis a unit vector
	float radius, half_width;
};

struct Body
{
	std::vector<Box> boxes;
	std::optional<Round> round;
};

struct Hinge
{
	size_t a, b;   // bodies
	v3f anchor;    // the assembly's frame
	v3f axis;      // unit
	float friction; // N·m that turning it costs (rolling resistance)
};

struct Assembly
{
	std::vector<Body> bodies; // the first is its root: its pose is the assembly's
	std::vector<Hinge> hinges;
};

struct Quat
{
	float x = 0, y = 0, z = 0, w = 1;
	bool operator==(const Quat &o) const { return x == o.x && y == o.y && z == o.z && w == o.w; }
};

struct Pose
{
	v3d position;
	Quat rotation;
	bool operator==(const Pose &o) const { return position == o.position && rotation == o.rotation; }
};

// A body's pose in its assembly's root's frame: how far a wheel has turned.
struct Relative
{
	v3f offset;
	Quat rotation;
};

enum class Tier : unsigned char {
	Live,   // in Jolt, stepped
	Frozen, // at rest and away from players: a pose, costing nothing
};

struct Capsule // a player as the server has them: placed where their client says
{
	v3d foot;   // the bottom of it
	float radius, height;
	float mass = 75, strength = 600; // kg; N they can push with
};

struct CharacterSpec
{
	float radius, height; // nodes: a capsule standing on its foot
	float mass;           // kg
	float strength;       // N it pushes with
	float max_slope;      // degrees it can stand on
};

class World
{
public:
	struct Opts
	{
		v3f gravity; // nodes/s²
		// The ground in block `block`: boxes in nodes from its least node's
		// centre. False: it isn't loaded (what's on it stays frozen).
		std::function<bool(v3s16 block, std::vector<aabb3f> &boxes)> ground;
		// How near a player keeps an assembly Live (nodes); it freezes at
		// rest past `freeze` (more: so it doesn't flicker at the edge).
		float live = 48, freeze = 56;
		float step_up = 0.6f; // nodes a character walks up without jumping
	};
	explicit World(const Opts &opts);
	~World();

	using Id = unsigned;
	// Throws PhysicsError on an assembly with no bodies, a body with no
	// boxes, or a hinge naming a body it hasn't.
	Id add(const Assembly &a, const Pose &pose, v3f velocity = v3f(0));
	void remove(Id id);
	bool has(Id id) const;
	// Its bodies remade (it was edited): where it is and how it moves kept.
	void rebuild(Id id, const Assembly &a);

	// Held: kinematic, it goes where `to` says (each step, at the speed
	// that gets it there); let go, it falls from there as it was moving.
	void hold(Id id, bool held);
	void moveTo(Id id, const Pose &to);
	void impulse(Id id, v3f impulse); // N·s at its root's centre of mass

	// The players this step, each a character put where they are (pushing
	// what's in the way as hard as they can); Live/Frozen follows them.
	void players(const std::vector<Capsule> &players);

	// Carried (a strength, N) or let go (none); while carried, pulled toward
	// `to` each step. Being carried isn't being held: it's not kinematic.
	void carry(Id id, std::optional<float> strength);
	void pullTo(Id id, const Pose &to);
	bool carried(Id id) const;
	bool held(Id id) const;

	using CharId = unsigned;
	CharId addCharacter(const CharacterSpec &spec, const v3d &foot);
	void removeCharacter(CharId id);
	// The velocity it walks at from the next step (nodes/s, gravity its own).
	void want(CharId id, v3f velocity);
	// Put somewhere, moving so (a teleport, or a hand-over from other movement).
	void place(CharId id, const v3d &foot, v3f velocity);
	struct CharacterState
	{
		v3d foot;
		v3f velocity;
		bool on_ground;      // standing on something it can stand on
		v3f ground_velocity; // of what it stands on (a cart it rides)
		v3f ground_normal;
	};
	CharacterState character(CharId id) const;
	// Where it's drawn now: between its last two steps by the time since.
	v3d drawn(CharId id) const;
	// A block of ground changed: built again, what's on it woken.
	void groundChanged(v3s16 block);

	// Advances by `dt` in fixed steps (at most a fifth of a second's: a
	// slow server drops time rather than falling further behind).
	void step(double dt);

	Pose pose(Id id) const;
	v3f velocity(Id id) const;
	float mass(Id id) const;
	Tier tier(Id id) const;
	bool asleep(Id id) const;
	// Each body but the root, in order: where it is in the root's frame.
	std::vector<Relative> relatives(Id id) const;

	// How many ground blocks are built (for tests: only round what's Live).
	size_t groundBlocks() const;

private:
	struct Impl;
	std::unique_ptr<Impl> m;
};

} // namespace physics
