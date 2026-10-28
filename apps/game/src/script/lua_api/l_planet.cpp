// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "lua_api/l_planet.h"
#include "common/c_converter.h"
#include "lua_api/l_internal.h"
#include "planet/atlas.h"
#include "emerge.h"
#include "mapgen/mapgen.h"
#include "server.h"
#include <cmath>

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
