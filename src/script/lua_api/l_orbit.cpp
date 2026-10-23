// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "lua_api/l_orbit.h"
#include "lua_api/l_internal.h"
#include "orbit/flight.h"
#include <cmath>

using orbit::Vec;

static const orbit::Flight &flight()
{
	static const orbit::Flight f(terragen::home());
	return f;
}

// a number field of the table at `index`; raises if missing or not finite
static double number(lua_State *L, int index, const char *key)
{
	lua_getfield(L, index, key);
	if (!lua_isnumber(L, -1))
		luaL_error(L, "core.orbit: field '%s' must be a number", key);
	double v = lua_tonumber(L, -1);
	lua_pop(L, 1);
	if (!std::isfinite(v))
		luaL_error(L, "core.orbit: field '%s' isn't finite", key);
	return v;
}

static Vec vec(lua_State *L, int index)
{
	if (index < 0) // LuaJIT has no lua_absindex
		index = lua_gettop(L) + index + 1;
	luaL_checktype(L, index, LUA_TTABLE);
	return { number(L, index, "x"), number(L, index, "y"), number(L, index, "z") };
}

static Vec vecField(lua_State *L, int index, const char *key)
{
	lua_getfield(L, index, key);
	if (!lua_istable(L, -1))
		luaL_error(L, "core.orbit: field '%s' must be a vector", key);
	Vec v = vec(L, -1);
	lua_pop(L, 1);
	return v;
}

static orbit::State state(lua_State *L, int index)
{
	luaL_checktype(L, index, LUA_TTABLE);
	return { vecField(L, index, "pos"), vecField(L, index, "vel") };
}

static orbit::Craft craft(lua_State *L, int index)
{
	luaL_checktype(L, index, LUA_TTABLE);
	orbit::Craft c{ number(L, index, "mass"), number(L, index, "drag_area") };
	if (c.mass <= 0 || c.drag_area < 0)
		luaL_error(L, "core.orbit: a craft needs mass > 0 and drag_area ≥ 0");
	return c;
}

static void setNumber(lua_State *L, const char *key, double value)
{
	lua_pushnumber(L, value);
	lua_setfield(L, -2, key);
}

static void pushVec(lua_State *L, const Vec &v)
{
	lua_createtable(L, 0, 3);
	setNumber(L, "x", v.x);
	setNumber(L, "y", v.y);
	setNumber(L, "z", v.z);
}

static void pushState(lua_State *L, const orbit::State &s)
{
	lua_createtable(L, 0, 2);
	pushVec(L, s.pos);
	lua_setfield(L, -2, "pos");
	pushVec(L, s.vel);
	lua_setfield(L, -2, "vel");
}

int ModApiOrbit::l_planet(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	const terragen::Planet &p = flight().planet();
	lua_createtable(L, 0, 6);
	setNumber(L, "radius", p.radius);
	setNumber(L, "gravity", p.gravity);
	setNumber(L, "mu", p.mu());
	setNumber(L, "day", p.day);
	setNumber(L, "density", p.density);
	setNumber(L, "scale_height", p.scale_height);
	return 1;
}

int ModApiOrbit::l_density(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	lua_pushnumber(L, flight().density(luaL_checknumber(L, 1)));
	return 1;
}

static int stepWith(lua_State *L, bool near)
{
	orbit::State s = state(L, 1);
	orbit::Craft c = craft(L, 2);
	Vec thrust = vec(L, 3);
	double dt = luaL_checknumber(L, 4);
	if (!(dt >= 0) || dt > 60)
		luaL_error(L, "core.orbit: a step is 0..60 s");
	pushState(L, near ? flight().near(s, c, thrust, dt) : flight().step(s, c, thrust, dt));
	return 1;
}

int ModApiOrbit::l_step(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	return stepWith(L, false);
}

int ModApiOrbit::l_near(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	return stepWith(L, true);
}

int ModApiOrbit::l_elements(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	orbit::Elements e = flight().elements(state(L, 1));
	lua_createtable(L, 0, 8);
	setNumber(L, "altitude", e.altitude);
	setNumber(L, "speed", e.speed);
	setNumber(L, "orbital_speed", e.orbital_speed);
	lua_pushboolean(L, e.bound);
	lua_setfield(L, -2, "bound");
	if (e.bound) {
		setNumber(L, "apoapsis", e.apoapsis);
		setNumber(L, "period", e.period);
	}
	setNumber(L, "periapsis", e.periapsis);
	setNumber(L, "inclination", e.inclination);
	return 1;
}

int ModApiOrbit::l_directions(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	orbit::Directions d = flight().directions(state(L, 1));
	lua_createtable(L, 0, 3);
	pushVec(L, d.prograde);
	lua_setfield(L, -2, "prograde");
	pushVec(L, d.radial);
	lua_setfield(L, -2, "radial");
	pushVec(L, d.normal);
	lua_setfield(L, -2, "normal");
	return 1;
}

static void pushWhere(lua_State *L, const orbit::Where &w)
{
	lua_createtable(L, 0, 4);
	setNumber(L, "latitude", w.latitude);
	setNumber(L, "longitude", w.longitude);
	setNumber(L, "altitude", w.altitude);
	pushVec(L, w.velocity);
	lua_setfield(L, -2, "velocity");
}

int ModApiOrbit::l_path(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	orbit::State s = state(L, 1);
	int points = (int)luaL_checkinteger(L, 2);
	if (points < 2 || points > 1024)
		luaL_error(L, "core.orbit: a path has 2..1024 points");
	std::vector<Vec> path = flight().path(s, points);
	lua_createtable(L, (int)path.size(), 0);
	for (size_t i = 0; i < path.size(); i++) {
		orbit::Where w = flight().where({ path[i], {} });
		lua_createtable(L, 0, 3);
		setNumber(L, "latitude", w.latitude);
		setNumber(L, "longitude", w.longitude);
		setNumber(L, "altitude", w.altitude);
		lua_rawseti(L, -2, (int)i + 1);
	}
	return 1;
}

int ModApiOrbit::l_where(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	pushWhere(L, flight().where(state(L, 1)));
	return 1;
}

int ModApiOrbit::l_at(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	luaL_checktype(L, 1, LUA_TTABLE);
	orbit::Where w{ number(L, 1, "latitude"), number(L, 1, "longitude"), number(L, 1, "altitude"), vecField(L, 1, "velocity") };
	pushState(L, flight().at(w));
	return 1;
}

void ModApiOrbit::Initialize(lua_State *L, int top)
{
	lua_newtable(L);
	int t = lua_gettop(L);
	luaL_Reg functions[] = {
		{ "planet", l_planet },
		{ "density", l_density },
		{ "step", l_step },
		{ "near", l_near },
		{ "elements", l_elements },
		{ "directions", l_directions },
		{ "path", l_path },
		{ "where", l_where },
		{ "at", l_at },
	};
	for (const luaL_Reg &f : functions) {
		lua_pushcfunction(L, f.func);
		lua_setfield(L, t, f.name);
	}
	lua_setfield(L, top, "orbit");
}
