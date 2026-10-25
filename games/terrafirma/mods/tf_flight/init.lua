-- Flight over the planet: a creative flight pack that flies from the ground
-- up through the air, round the planet in orbit and back down. The physics
-- is the engine's (core.planet.fly over core.orbit: the planet's gravity,
-- weakening with height, its air, its turning).
--
-- - craft.lua: the pack's craft, which the rider sits in.
-- - body.lua: how the engine moves a player: walking (under gravity that
--   weakens as they climb) or riding.

-- Worlds terra didn't generate (the Lab) have no planet to fly.
if not core.terra.enabled() then
	return
end

local path = core.get_modpath("tf_flight")

local PACK = {
	craft = { mass = 100, drag_area = 0.7 }, -- a person, roughly
	thrust = 3 * 9.81 * 100,                 -- N: a brisk climb, and orbit in a couple of minutes' burn
	boost = 4,                               -- Ctrl held: 12 g
}

local planet = core.orbit.planet()
local sea = tonumber(core.get_mapgen_setting("water_level")) or error("tf_flight: no water_level")
-- where the air ends: 8 of its scale heights up, it's e⁻⁸ of the ground's;
-- above it the page shows the orbit
local BOUNDARY = 8 * planet.scale_height

local body = dofile(path .. "/body.lua")({ sea = sea })
local craft = dofile(path .. "/craft.lua")({ body = body, pack = PACK, sea = sea, boundary = BOUNDARY })

core.register_globalstep(function()
	for _, player in ipairs(core.get_connected_players()) do
		body.step(player)
	end
end)

-- ---- the pack ---------------------------------------------------------------

local function strap(stack, user)
	local parent = user:get_attach()
	local e = parent and parent:get_luaentity()
	if e and e.name == "tf_flight:craft" then
		e:dismount(user)
	elseif not parent then
		craft.launch(user)
	end
end

core.register_tool("tf_flight:pack", {
	description = "Flight Pack\nRight-click to strap in or out. W/S forward and back where you look, A/D sideways, "
		.. "Space up, Ctrl for more. On the ground S brakes; in the air Shift goes down.\n"
		.. "The air ends " .. math.floor(BOUNDARY) .. " m up: burn east above it to go round the planet.",
	inventory_image = "tf_flight_pack.png",
	groups = { creative_functional = 1 },
	on_secondary_use = strap,
	on_place = strap,
})

-- Someone who leaves mid-flight is still flying when they come back.
core.register_on_joinplayer(function(player)
	local flying = player:get_meta():get_string("tf_flight")
	if flying ~= "" and flying ~= "flying" then
		error("tf_flight: " .. player:get_player_name() .. " has a flight saved as '" .. flying .. "'")
	end
	core.after(0, function()
		if player:is_valid() and flying == "flying" then craft.launch(player) end
	end)
end)

core.register_on_leaveplayer(function(player)
	body.forget(player)
end)

-- For the sky and the checks.
tf_flight = { boundary = BOUNDARY, sea = sea, pack = PACK }
