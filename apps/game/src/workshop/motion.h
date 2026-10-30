// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "orbit/field.h"

#include "irr_v3d.h"
#include "mapnode.h"
#include "physics/world.h"
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

class IGameDef;
class Map;
class NodeDefManager;
class LuaEntitySAO;
class ServerEnvironment;
struct MapEditEvent;
namespace piece { class Machine; }

namespace workshop {

// What a block is as a thing that moves: from its matter (Lua says, by
// core.physics.material).
struct Material
{
	float density;  // kg/m³
	float friction; // Coulomb
};

// How a machine is held (TOSERVER_OBJECT_HOLD's u8): not at all, still
// where its holder poses it (the workbench), or carried (pulled toward the
// hands as hard as the carrier can: sprint 58).
enum class Grip : u8 { Free = 0, Still = 1, Carried = 2 };

// How hard a player pulls what they carry (N): in creative, as hard as
// they like; else as a strong person (about 150 kg held up).
constexpr float STRENGTH_CREATIVE = 1e6f, STRENGTH = 1500;

// The world's blocks in map block `block` as boxes (nodes, from its least
// node's centre), as things that move meet them: cut and smoothed shapes as
// they're drawn, whole blocks inside the ground left out. False: it isn't
// loaded. The server's physics and a client's use it alike.
bool groundBoxes(Map &map, const NodeDefManager *ndef, v3s16 block, std::vector<aabb3f> &boxes);

// Every block's material, by content id: declared as mods load, before
// there's a world to move (Server owns it; Motion reads it).
class Materials
{
public:
	// Throws std::invalid_argument on a material that isn't one.
	void set(content_t c, const Material &m);
	const Material *get(content_t c) const;
	// Each content id that has one, with it.
	std::vector<std::pair<content_t, Material>> all() const;

private:
	std::vector<std::optional<Material>> m_by_content;
};

/*
	Motion (sprint 57): machines moved by physics. Each machine entity in
	the world is an assembly of physics::World: each rigid group of its
	parts (Machine::rigid) a body of its built blocks' boxes, weighed by
	their materials; each hinge a hinge, its friction rolling resistance.
	A hinged group that's round about its axle rolls as a smooth cylinder.

	It drives its entities (LuaEntitySAO::setDriven): their position,
	turn and velocity are the physics', and their groups' poses go to
	clients (AO_CMD_SET_PARTS). A machine edited is rebuilt where it is;
	held, it's kinematic, where its holder poses it. Players push.
*/
// The physics' field from the planet's (it's kept: the field outlives the world).
physics::Field fieldOf(const orbit::Field &field);

class Motion
{
public:
	Motion(ServerEnvironment &env, IGameDef *gamedef, const Materials &materials, const orbit::Field &field);
	~Motion();

	// Every machine in the world found, built, stepped and drawn there.
	void step(float dtime);
	void mapEdited(const MapEditEvent &e);

	// Its entity's machine is held (still, posed by its holder) or let go.
	void hold(LuaEntitySAO &obj, bool held);
	bool held(const LuaEntitySAO &obj) const;
	// Carried (pulled as hard as `strength`, N) or not (none).
	void carry(LuaEntitySAO &obj, std::optional<float> strength);
	bool carried(const LuaEntitySAO &obj) const;
	// Where a held or carried one's to be (BS, degrees); throws if it's neither.
	void pose(LuaEntitySAO &obj, v3f position, v3f rotation);
	// Grabbed by `who` (their object id) at `point` (BS, in its frame) as
	// hard as `strength` (N), that point pulled toward `hand` (BS); or
	// let go (none). Sprint 68.
	struct Grab
	{
		v3f point;
		v3f hand;
		float strength;
	};
	void grab(LuaEntitySAO &obj, u16 who, std::optional<Grab> grab);
	// Whether `who` has it grabbed.
	bool grabbing(const LuaEntitySAO &obj, u16 who) const;

	// For core.physics: nothing if it isn't a machine physics moves.
	std::optional<v3f> velocity(u16 object) const; // BS/s
	std::optional<float> mass(u16 object) const;   // kg
	// kg·BS/s, from where its giver stands (BS) if it's given; throws on
	// what isn't one
	void impulse(u16 object, v3f impulse, std::optional<v3f> from = std::nullopt);

private:
	struct Tracked
	{
		physics::World::Id id;
		size_t text;    // its piece text's hash: when it changes, it's rebuilt
		std::string parts_sent;
	};

	bool ground(v3s16 block, std::vector<aabb3f> &boxes);
	Tracked *tracked(u16 object);

	ServerEnvironment &m_env;
	IGameDef *m_gamedef;
	std::unique_ptr<physics::World> m_world;
	const Materials &m_materials;
	std::map<u16, Tracked> m_tracked;                 // by object id
	std::map<u16, size_t> m_broken;                   // can't be built (its text's hash): said once
	std::set<u16> m_held;                             // held, tracked yet or not
	std::map<u16, float> m_carried;                   // carried, how hard, tracked yet or not
	std::map<u16, std::map<u16, Grab>> m_grabbed;     // grabbed, by whom, tracked yet or not
	void grip(u16 object, u16 who, const Grab &g, bool fresh);
};

// A machine compiled as physics sees it (its frame: nodes, from its node
// (0, 0, 0)'s centre): each rigid group a body of its built blocks' boxes,
// each hinge between groups a hinge (a round group about it rolls as a
// cylinder). Throws std::runtime_error naming a block with no material.
physics::Assembly compile(const piece::Machine &m, const Materials &materials, IGameDef *gamedef);

// A turn as the server sets an entity's (degrees: piece::frame) and back.
physics::Quat quatOf(v3f rotation);
v3f rotationOf(const physics::Quat &q);

} // namespace workshop
