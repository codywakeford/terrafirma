// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "lua_api/l_planet.h"
#include "common/c_converter.h"
#include "lua_api/l_internal.h"
#include "planet/atlas.h"
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

static void pushRect(lua_State *L, const planet::Atlas::Rect &r)
{
	lua_createtable(L, 0, 4);
	lua_pushinteger(L, r.x);
	lua_setfield(L, -2, "x");
	lua_pushinteger(L, r.z);
	lua_setfield(L, -2, "z");
	lua_pushinteger(L, r.width);
	lua_setfield(L, -2, "width");
	lua_pushinteger(L, r.depth);
	lua_setfield(L, -2, "depth");
}

// The region kinds, by terragen::Atlas::Region.
static const char *const REGIONS[] = { "segment", "cap", "core" };

// core.planet.atlas() → nil, or { code, radius, core, top, regions }: the
// atlas's mapping code; its radius and the core's (blocks); one past its
// highest layer; and where each region's grid is stored: regions, each
// { kind = "segment" | "cap" | "core", index, x, z, width, depth, y0, y1 }
// (y0..y1 storage y, exclusive).
int ModApiPlanet::l_atlas(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	const planet::Atlas *a = getServer(L)->atlas();
	if (!a) {
		lua_pushnil(L);
		return 1;
	}
	const terragen::Atlas &atlas = a->atlas();
	lua_createtable(L, 0, 6);
	lua_pushstring(L, atlas.code().c_str());
	lua_setfield(L, -2, "code");
	lua_pushnumber(L, atlas.params().radius);
	lua_setfield(L, -2, "radius");
	lua_pushnumber(L, atlas.params().core);
	lua_setfield(L, -2, "core");
	lua_pushinteger(L, atlas.params().height);
	lua_setfield(L, -2, "top");
	lua_createtable(L, (int)a->regions().size(), 0);
	int n = 1;
	for (const planet::Atlas::Region &r : a->regions()) {
		pushRect(L, r.rect);
		lua_pushstring(L, REGIONS[(int)r.kind]);
		lua_setfield(L, -2, "kind");
		lua_pushinteger(L, r.index);
		lua_setfield(L, -2, "index");
		lua_pushinteger(L, r.k0);
		lua_setfield(L, -2, "y0");
		lua_pushinteger(L, r.k1 + 1);
		lua_setfield(L, -2, "y1");
		lua_rawseti(L, -2, n++);
	}
	lua_setfield(L, -2, "regions");
	return 1;
}

// core.planet.atlas_place(pos) → nil (seam, the core, not an atlas), or
// latitude, longitude (radians): where a node's column is.
int ModApiPlanet::l_atlas_place(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	const planet::Atlas *a = getServer(L)->atlas();
	const v3s16 p = check_v3s16(L, 1);
	const auto at = a ? a->place({ p.X, p.Y, p.Z }) : std::nullopt;
	if (!at) {
		lua_pushnil(L);
		return 1;
	}
	lua_pushnumber(L, (*at)[0]);
	lua_pushnumber(L, (*at)[1]);
	return 2;
}

// core.planet.atlas_point(pos) → nil, or { x, y, z }: where a storage
// position (fractional) is on the planet, blocks from its centre, in the
// grid of the node it's in.
int ModApiPlanet::l_atlas_point(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	const planet::Atlas *a = getServer(L)->atlas();
	const v3f p = check_v3f(L, 1);
	const v3s16 n(std::lround(p.X), std::lround(p.Y), std::lround(p.Z));
	const auto at = a ? a->point({ n.X, n.Y, n.Z }, p.X, p.Y, p.Z) : std::nullopt;
	if (!at) {
		lua_pushnil(L);
		return 1;
	}
	lua_createtable(L, 0, 3);
	lua_pushnumber(L, at->x);
	lua_setfield(L, -2, "x");
	lua_pushnumber(L, at->y);
	lua_setfield(L, -2, "y");
	lua_pushnumber(L, at->z);
	lua_setfield(L, -2, "z");
	return 1;
}

// core.planet.atlas_node(latitude, longitude, layer) → pos, or nil if the
// cell there isn't stored: the node of the cell at that place, that layer up
// (every grid's layer k is the sphere R + k, so the cell is that layer's).
int ModApiPlanet::l_atlas_node(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	const planet::Atlas *a = getServer(L)->atlas();
	if (!a)
		throw LuaError("core.planet.atlas_node: this world isn't an atlas planet");
	const double lat = luaL_checknumber(L, 1), lon = luaL_checknumber(L, 2);
	const lua_Integer layer = luaL_checkinteger(L, 3);
	const terragen::Atlas &atlas = a->atlas();
	if (layer >= atlas.params().height) {
		lua_pushnil(L);
		return 1;
	}
	const double r = atlas.params().radius + layer + 0.5;
	if (r <= 0) {
		lua_pushnil(L); // past the centre
		return 1;
	}
	const planet::Atlas::Cell c = atlas.locate({ r * std::cos(lat) * std::cos(lon), r * std::sin(lat),
			r * std::cos(lat) * std::sin(lon) });
	const planet::Atlas::Node n = a->node(c);
	push_v3s16(L, v3s16(n.x, n.y, n.z));
	return 1;
}

// core.planet.atlas_cell(pos) → nil (seam, or not an atlas world), or
// { region = "segment" | "cap" | "core", index, i, j, k }: the cell the
// node stores, as terragen::Atlas addresses it.
int ModApiPlanet::l_atlas_cell(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	const planet::Atlas *a = getServer(L)->atlas();
	const v3s16 p = check_v3s16(L, 1);
	const std::optional<planet::Atlas::Cell> c = a ? a->cell({ p.X, p.Y, p.Z }) : std::nullopt;
	if (!c) {
		lua_pushnil(L);
		return 1;
	}
	lua_createtable(L, 0, 5);
	lua_pushstring(L, REGIONS[(int)c->region]);
	lua_setfield(L, -2, "region");
	lua_pushinteger(L, c->index);
	lua_setfield(L, -2, "index");
	for (const auto &[name, v] : { std::pair{ "i", c->i }, { "j", c->j }, { "k", c->k } }) {
		lua_pushinteger(L, v);
		lua_setfield(L, -2, name);
	}
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
		{ "atlas", l_atlas },
		{ "atlas_cell", l_atlas_cell },
		{ "atlas_place", l_atlas_place },
		{ "atlas_node", l_atlas_node },
		{ "atlas_point", l_atlas_point },
	};
	for (const luaL_Reg &f : functions) {
		lua_pushcfunction(L, f.func);
		lua_setfield(L, t, f.name);
	}
	lua_setfield(L, top, "planet");
}
