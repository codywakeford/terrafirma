// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "lua_api/l_planet.h"
#include "common/c_converter.h"
#include "lua_api/l_internal.h"
#include "planet/planet.h"
#include "planet/flying.h"
#include "emerge.h"
#include "mapgen/mapgen.h"
#include "server.h"
#include <cmath>

static const planet::Planet &planetOf(lua_State *L)
{
	const planet::Planet *p = ModApiBase::getServer(L)->planet();
	if (!p)
		throw LuaError("core.planet: this world isn't a planet (core.planet.enabled() is false)");
	return *p;
}

// core.planet.enabled() → whether the world is a planet.
int ModApiPlanet::l_enabled(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	lua_pushboolean(L, getServer(L)->planet() != nullptr);
	return 1;
}

// core.planet.half() → blocks from a face's centre to its edge.
int ModApiPlanet::l_half(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	lua_pushinteger(L, planetOf(L).half());
	return 1;
}

// core.planet.face(pos) → the face (0–5) a node is on, or nil in a gap
// between faces or off the planet.
int ModApiPlanet::l_face(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	const v3s16 p = check_v3s16(L, 1);
	const std::optional<int> face = planetOf(L).face(p.X, p.Z);
	if (face)
		lua_pushinteger(L, *face);
	else
		lua_pushnil(L);
	return 1;
}

// core.planet.square(face) → { x, z, width }: the face's lowest node and
// its width, in the map.
int ModApiPlanet::l_square(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	const planet::Planet &p = planetOf(L);
	const int face = luaL_checkinteger(L, 1);
	if (face < 0 || face >= 6)
		throw LuaError("core.planet.square: no face " + std::to_string(face));
	const planet::Planet::Square s = p.square(face);
	lua_createtable(L, 0, 3);
	setintfield(L, -1, "x", s.x);
	setintfield(L, -1, "z", s.z);
	setintfield(L, -1, "width", p.width());
	return 1;
}

// core.planet.step(pos, dir) → { pos, face, turn }: the node next to pos
// along dir (one of ±x, ±z), across a face's edge onto the next face if
// need be; turn: quarter turns the grid took (east toward north).
int ModApiPlanet::l_step(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	const planet::Planet &p = planetOf(L);
	const v3s16 pos = check_v3s16(L, 1), dir = check_v3s16(L, 2);
	if (dir.Y != 0 || std::abs(dir.X) + std::abs(dir.Z) != 1)
		throw LuaError("core.planet.step: dir is one of ±x, ±z");
	if (!p.face(pos.X, pos.Z))
		throw LuaError("core.planet.step: the position is on no face");
	const planet::Planet::Step s = p.step(pos.X, pos.Z, dir.X, dir.Z);
	lua_createtable(L, 0, 3);
	push_v3s16(L, v3s16(s.x, pos.Y, s.z));
	lua_setfield(L, -2, "pos");
	setintfield(L, -1, "face", s.face);
	setintfield(L, -1, "turn", s.turn);
	return 1;
}

// a number field of a table; raises if it's missing
static double field(lua_State *L, int index, const char *key)
{
	lua_getfield(L, index, key);
	if (!lua_isnumber(L, -1))
		throw LuaError(std::string("core.planet.fly: field '") + key + "' must be a number");
	const double v = lua_tonumber(L, -1);
	lua_pop(L, 1);
	return v;
}

// core.planet.fly(pos, vel, craft, thrust, dt) → { pos, vel, turn }: a craft
// flown over the planet for dt seconds under its gravity and air, pushed by
// `thrust` (N). pos: the map's (y the map's height); vel and thrust: along
// the grid where it is (x east, y up, z north), m/s and N; craft: { mass,
// drag_area }. Over a face's edge it lands on the next face (pos), its
// velocity in that face's grid, and `turn` is the quarter turns the grid took.
int ModApiPlanet::l_fly(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	Server *server = getServer(L);
	const planet::Planet &p = planetOf(L);
	const double sea = server->getEmergeManager()->mgparams->water_level;
	luaL_checktype(L, 1, LUA_TTABLE);
	luaL_checktype(L, 2, LUA_TTABLE);
	luaL_checktype(L, 3, LUA_TTABLE);
	luaL_checktype(L, 4, LUA_TTABLE);
	const planet::Flying::Local from{ field(L, 1, "x"), field(L, 1, "z"), field(L, 1, "y") - sea,
		field(L, 2, "x"), field(L, 2, "y"), field(L, 2, "z") };
	const orbit::Craft craft{ field(L, 3, "mass"), field(L, 3, "drag_area") };
	const std::array<double, 3> thrust{ field(L, 4, "x"), field(L, 4, "y"), field(L, 4, "z") };
	const double dt = luaL_checknumber(L, 5);
	const planet::Flying fly(p, server->flight());
	const planet::Flying::Step s = fly.step(from, craft, thrust, dt);
	lua_createtable(L, 0, 3);
	push_v3f(L, v3f(s.where.x, s.where.altitude + sea, s.where.z));
	lua_setfield(L, -2, "pos");
	push_v3f(L, v3f(s.where.east, s.where.up, s.where.north));
	lua_setfield(L, -2, "vel");
	setintfield(L, -1, "turn", s.turn);
	return 1;
}

// core.planet.state(pos, vel) → { pos, vel }: where something at pos moving
// at vel (along the grid there, m/s) is in core.orbit's frame (planet-centred
// metres), for core.orbit.elements and the like.
int ModApiPlanet::l_state(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	Server *server = getServer(L);
	const planet::Planet &p = planetOf(L);
	const double sea = server->getEmergeManager()->mgparams->water_level;
	luaL_checktype(L, 1, LUA_TTABLE);
	luaL_checktype(L, 2, LUA_TTABLE);
	const planet::Flying fly(p, server->flight());
	const orbit::State s = fly.toOrbit({ field(L, 1, "x"), field(L, 1, "z"), field(L, 1, "y") - sea,
		field(L, 2, "x"), field(L, 2, "y"), field(L, 2, "z") });
	const auto vec = [&](const orbit::Vec &v) {
		lua_createtable(L, 0, 3);
		lua_pushnumber(L, v.x);
		lua_setfield(L, -2, "x");
		lua_pushnumber(L, v.y);
		lua_setfield(L, -2, "y");
		lua_pushnumber(L, v.z);
		lua_setfield(L, -2, "z");
	};
	lua_createtable(L, 0, 2);
	vec(s.pos);
	lua_setfield(L, -2, "pos");
	vec(s.vel);
	lua_setfield(L, -2, "vel");
	return 1;
}

void ModApiPlanet::Initialize(lua_State *L, int top)
{
	lua_newtable(L);
	int t = lua_gettop(L);
	luaL_Reg functions[] = {
		{ "enabled", l_enabled },
		{ "half", l_half },
		{ "face", l_face },
		{ "square", l_square },
		{ "step", l_step },
		{ "fly", l_fly },
		{ "state", l_state },
	};
	for (const luaL_Reg &f : functions) {
		lua_pushcfunction(L, f.func);
		lua_setfield(L, t, f.name);
	}
	lua_setfield(L, top, "planet");
}
