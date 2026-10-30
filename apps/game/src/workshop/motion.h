// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

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

// Every block's material, by content id: declared as mods load, before
// there's a world to move (Server owns it; Motion reads it).
class Materials
{
public:
	// Throws std::invalid_argument on a material that isn't one.
	void set(content_t c, const Material &m);
	const Material *get(content_t c) const;

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
class Motion
{
public:
	Motion(ServerEnvironment &env, IGameDef *gamedef, const Materials &materials);
	~Motion();

	// Every machine in the world found, built, stepped and drawn there.
	void step(float dtime);
	void mapEdited(const MapEditEvent &e);

	// Its entity's machine is held (still, posed by its holder) or let go.
	void hold(LuaEntitySAO &obj, bool held);
	bool held(const LuaEntitySAO &obj) const;
	// Where a held one's to be (BS, degrees); throws if it isn't held.
	void pose(LuaEntitySAO &obj, v3f position, v3f rotation);

	// For core.physics: nothing if it isn't a machine physics moves.
	std::optional<v3f> velocity(u16 object) const; // BS/s
	std::optional<float> mass(u16 object) const;   // kg
	void impulse(u16 object, v3f impulse);         // kg·BS/s; throws on what isn't one

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
