-- Flight, and space as a place. The physics is the engine's (core.orbit:
-- the planet's gravity and air, the same as the wiki's cannonball).
--
-- - space.lua: above the boundary, everything rides in bubbles: cells of
--   the map past the polar wall, each falling around the planet on an orbit.
-- - craft.lua: the flight pack's craft, over the blocks or in a bubble.
-- - body.lua: how the engine moves a player: walking, riding, floating.
--
-- Anyone crossing the boundary goes to space, pack or not; with no sideways
-- speed to speak of they fall straight back.

-- Worlds terra didn't generate (the Lab) have no planet to fly.
if not core.terra.enabled() then
	return
end

local path = core.get_modpath("tf_flight")

local SAVE = 2         -- s between saving the bubbles
local PACK = {
	craft = { mass = 100, drag_area = 0.7 }, -- a person, roughly
	thrust = 3 * 9.81 * 100,                 -- N: a brisk climb, and orbit in a couple of minutes' burn
	boost = 4,                               -- Ctrl held: 12 g, up through the air in about a minute
}

local planet = core.orbit.planet()
local world = core.terra.world()
local BOUNDARY = world.boundary -- m above the sea, where space begins
local sea = tonumber(core.get_mapgen_setting("water_level")) or error("tf_flight: no water_level")
if sea + BOUNDARY >= 31000 then error("tf_flight: the boundary is above the map's top") end

local body = dofile(path .. "/body.lua")({ space = world.space })
local space = dofile(path .. "/space.lua")({
	boundary = BOUNDARY, sea = sea, space = world.space, lone = PACK.craft, store = core.get_mod_storage(),
})
local craft = dofile(path .. "/craft.lua")({ space = space, body = body, pack = PACK, sea = sea })

-- ---- each step --------------------------------------------------------------

-- What moves a player in space: their craft, or themselves.
local function carrier(player)
	return player:get_attach() or player
end

-- The page's reading for a player in a bubble.
local function reading(b, obj)
	local s = space.locate(b, obj)
	local e = core.orbit.elements(s)
	return {
		regime = "space",
		boundary = BOUNDARY,
		radius = planet.radius,
		altitude = e.altitude,
		speed = e.speed,
		orbital_speed = e.orbital_speed,
		climb = core.orbit.where(s).velocity.y,
		apoapsis = e.apoapsis,
		periapsis = e.periapsis,
		period = e.period,
		inclination = e.inclination,
		density = core.orbit.density(e.altitude),
		riding = not obj:is_player(),
		lone = space.lone(b, obj),
		buildable = space.buildable(b),
	}
end

-- A player on the planet above the boundary (on foot, flying in creative):
-- into space with the speed they have (the engine's, in blocks).
local function rise(player)
	local pos, vel = player:get_pos(), player:get_velocity()
	local a = core.terra.across(pos.y - sea, pos.z)
	space.enter(player, space.up({
		pos = { x = pos.x, y = pos.y - sea, z = pos.z },
		vel = { x = vel.x * a.east, y = vel.y, z = vel.z * a.north },
	}))
end

-- Where a player is over the planet: map x, z and m above the sea, with
-- their rates (blocks/s, blocks/s, m/s). In a bubble, its orbit says;
-- otherwise where they stand.
local function aloft(player)
	local pos = player:get_pos()
	local b = pos.z >= world.space and space.at(pos)
	if not b then
		local vel = player:get_velocity()
		return { x = pos.x, z = pos.z, altitude = pos.y - sea, east = vel.x, north = vel.z, climb = vel.y }
	end
	local w = core.orbit.where(space.locate(b, carrier(player)))
	local m = core.terra.block(w.latitude, w.longitude)
	local a = core.terra.across(w.altitude, m.z)
	return { x = m.x, z = m.z, altitude = w.altitude,
		east = w.velocity.x / a.east, north = w.velocity.z / a.north, climb = w.velocity.y }
end

-- A player in space back on the orbit they were saved on: rejoining (their
-- bubble emptied when they left, its cell maybe someone else's now), or
-- found with no bubble.
local function restore(player)
	local saved = player:get_meta():get_string("tf_space")
	if saved == "" then error("tf_flight: " .. player:get_player_name() .. " is in space with no orbit saved") end
	space.enter(carrier(player), core.deserialize(saved) or error("tf_flight: a saved orbit can't be read"))
end

local above = {} -- player name → true while the far view is told where they are (set_aloft)
local saving = 0
core.register_globalstep(function(dtime)
	space.step(dtime)
	saving = saving + dtime
	local save = saving >= SAVE
	if save then saving = 0 end
	for _, player in ipairs(core.get_connected_players()) do
		local pos = player:get_pos()
		if pos.z >= world.space then
			local b = space.at(pos)
			if not b then
				restore(player)
			else
				local obj = carrier(player)
				player:set_ui("flight", reading(b, obj))
				player:get_meta():set_string("tf_space", core.serialize(space.locate(b, obj)))
				player:set_aloft(aloft(player))
				above[player:get_player_name()] = true
			end
		else
			if above[player:get_player_name()] then
				player:set_aloft(nil)
				above[player:get_player_name()] = nil
			end
			if not player:get_attach() then
				if pos.y > sea + BOUNDARY then rise(player) end
				player:set_ui("flight", nil)
			end
		end
		body.step(player, dtime)
	end
	if save then space.save() end
end)

-- ---- building in space ------------------------------------------------------

-- Blocks go only in a bubble whose orbit holds (space.buildable): nowhere
-- else past the polar wall.
local protected = core.is_protected
function core.is_protected(pos, name)
	if pos.z >= world.space then
		local b = space.at(pos)
		if not b then return true end
		if not space.buildable(b) then
			core.chat_send_player(name, "This orbit comes back down, and blocks can't come down with it yet: "
				.. "raise its lowest point above " .. BOUNDARY / 1000 .. " km to build here.")
			return true
		end
	end
	return protected(pos, name)
end

core.register_on_placenode(function(pos)
	local b = pos.z >= world.space and space.at(pos)
	if b then space.build(b) end
end)

-- ---- the pack ---------------------------------------------------------------

local function strap(stack, user)
	local parent = user:get_attach()
	local e = parent and parent:get_luaentity()
	if e and e.name == "tf_flight:craft" then
		e:dismount(user)
	elseif not parent then
		craft.launch(user, user:get_pos().z >= world.space and "space" or "near")
	end
end

core.register_tool("tf_flight:pack", {
	description = "Flight Pack\nRight-click to strap in or out. W/S forward and back where you look, A/D sideways, "
		.. "Space up, Ctrl for more. Over the ground S brakes; in space Shift goes down, and Ctrl coasting alone runs time ×20.\n"
		.. "Space begins " .. BOUNDARY / 1000 .. " km up.",
	inventory_image = "tf_flight_pack.png",
	groups = { creative_functional = 1 },
	on_secondary_use = strap,
	on_place = strap,
})

-- Someone who leaves in space, or mid-flight, is still there when they come back.
core.register_on_joinplayer(function(player)
	local meta = player:get_meta()
	local regime = meta:get_string("tf_flight")
	-- TEMP(2026-10-23): flights saved before space bubbles were a serialized
	-- table of the old regimes; they can't resume. Remove once played worlds are past it.
	if regime:sub(1, 6) == "return" then
		core.log("action", "tf_flight: dropping " .. player:get_player_name() .. "'s flight saved in the old form")
		meta:set_string("tf_flight", "")
		regime = ""
	end
	if regime ~= "" and regime ~= "near" and regime ~= "space" then
		error("tf_flight: " .. player:get_player_name() .. " has a flight saved as '" .. regime .. "'")
	end
	core.after(0, function()
		if not player:is_valid() then return end
		if player:get_pos().z >= world.space then restore(player) end
		if regime ~= "" then craft.launch(player, regime) end
	end)
end)

core.register_on_leaveplayer(function(player)
	body.forget(player)
	above[player:get_player_name()] = nil
end)

-- For the flight check (scripts/flight-check.sh), and the sky (tf_sky: aloft).
tf_flight = { boundary = BOUNDARY, sea = sea, pack = PACK, space = space, near = craft.near, aloft = aloft }
