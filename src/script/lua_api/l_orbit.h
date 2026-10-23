// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "lua_api/l_base.h"

/*
	core.orbit: flight on the world's planet (src/orbit). Pure: content keeps
	each craft's state and steps it. Positions are planet-centred metres in
	the frame turning with the planet (+y north); `near` is the flat patch of
	blocks (x east, y up from sea level, z north).

	  planet() → { radius, gravity, mu, day, density, scale_height }
	  density(altitude) → kg/m³
	  step(state, craft, thrust, dt) → state     state = { pos = v, vel = v }, craft = { mass, drag_area },
	  near(state, craft, thrust, dt) → state     thrust a vector in newtons
	  elements(state) → { altitude, speed, orbital_speed, bound, apoapsis, periapsis, period, inclination }
	  directions(state) → { prograde, radial, normal }   unit vectors to aim a burn by
	  path(state, points) → { { latitude, longitude, altitude }, … }   the orbit now, no drag
	  where(state) → { latitude, longitude, altitude, velocity = { x east, y up, z north } }
	  at(where) → state
*/
class ModApiOrbit : public ModApiBase
{
private:
	static int l_planet(lua_State *L);
	static int l_density(lua_State *L);
	static int l_step(lua_State *L);
	static int l_near(lua_State *L);
	static int l_elements(lua_State *L);
	static int l_directions(lua_State *L);
	static int l_path(lua_State *L);
	static int l_where(lua_State *L);
	static int l_at(lua_State *L);

public:
	static void Initialize(lua_State *L, int top);
};
