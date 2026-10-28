-- Flight over the planet: for now, the body that walks on it (body.lua:
-- gravity that weakens as a player climbs). The flight pack flew the cube
-- planet's orbits and went with it; flight returns with the system's
-- orbits (planning/sprint-31-planetarium.md).

-- Worlds terra didn't generate (the Lab) have no planet to walk.
if not core.terra.enabled() then
	return
end

local sea = tonumber(core.get_mapgen_setting("water_level")) or error("tf_flight: no water_level")
local body = dofile(core.get_modpath("tf_flight") .. "/body.lua")({ sea = sea })

core.register_globalstep(function()
	for _, player in ipairs(core.get_connected_players()) do
		body.step(player)
	end
end)

core.register_on_leaveplayer(function(player)
	body.forget(player)
end)
