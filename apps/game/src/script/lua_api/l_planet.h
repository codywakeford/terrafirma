// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "lua_api/l_base.h"

/*
	core.planet: the planet the world is (src/planet/planet.h), for game Lua.
	Server only. On a flat world every call but enabled() raises an error.
	(Phase 1 of planning/sprint-26-planet.md: what the game needs to keep the
	gaps between faces; phase 3 adds stepping across edges.)

	An atlas planet (src/planet/atlas.h, planning/sprint-30-atlas.md) isn't
	a cube one: enabled() is false and the face calls raise; atlas() and
	atlas_cell() answer instead (and return nil on any other world).
*/
class ModApiPlanet : public ModApiBase
{
private:
	static int l_enabled(lua_State *L);
	static int l_half(lua_State *L);
	static int l_face(lua_State *L);
	static int l_square(lua_State *L);
	static int l_step(lua_State *L);
	static int l_fly(lua_State *L);
	static int l_state(lua_State *L);
	static int l_atlas(lua_State *L);
	static int l_atlas_cell(lua_State *L);
	static int l_atlas_place(lua_State *L);
	static int l_atlas_node(lua_State *L);
	static int l_atlas_point(lua_State *L);

public:
	static void Initialize(lua_State *L, int top);
};
