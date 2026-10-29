-- /travel: to another body of the world (planning/sprint-40-bodies.md, step
-- 6). Behind a loading screen (the page's "travel"): the player held where
-- they are while the land there is made, then put on it. Getting there
-- without a screen (leave, fly, arrive) is sprint 41's.

local HOLD = 1.5 -- seconds on the screen after arriving, for the blocks round them to reach the client
local REACH = 32 -- nodes round the landing made before it

local travelling = {} -- player name → the physics they had

-- Land on a body near a place (degrees; its equator from longitude 0 when
-- none): the place itself, else the nearest in rings round it, a degree
-- apart. Nil when there's none within 30°.
local function landing(body, lat, lon)
	if not lat then return tf_planet.spawn_position(body) end
	for ring = 0, 30 do
		for step = 0, math.max(0, ring * 8 - 1) do
			local a = ring == 0 and 0 or step / (ring * 8) * 2 * math.pi
			local la = math.max(-85, math.min(85, lat + ring * math.sin(a)))
			local lo = lon + ring * math.cos(a)
			local layer = core.planet.spawn_level(math.rad(la), math.rad(lo), body)
			local node = layer and core.planet.atlas_node(math.rad(la), math.rad(lo), layer, body)
			if node then return node end
		end
	end
end

local function release(player)
	local name = player:get_player_name()
	if not travelling[name] then return end
	player:set_physics_override(travelling[name])
	travelling[name] = nil
	player:set_ui("travel", nil)
end

-- Travel a player to a body: true, or false and why not.
function tf_planet.travel(player, body, lat, lon)
	local name = player:get_player_name()
	if travelling[name] then return false, "already on the way" end
	local known
	for _, b in ipairs(core.planet.bodies() or {}) do
		if b.id == body then known = b end
	end
	if not known then
		local ids = {}
		for _, b in ipairs(core.planet.bodies() or {}) do ids[#ids + 1] = b.id end
		if #ids == 1 then
			-- every body at the home's scale: on a small world the others are too small to stand on
			return false, "no body called " .. body .. ": this world is too small for any other (a moon needs a planet 10k round or more)"
		end
		return false, "no body called " .. body .. "; this world has: " .. table.concat(ids, ", ")
	end
	local node = landing(body, lat, lon)
	if not node then return false, "no land on " .. known.name .. " within 30° of there" end
	travelling[name] = player:get_physics_override()
	player:set_physics_override({ speed = 0, jump = 0, gravity = 0 })
	player:set_ui("travel", { body = known.id, name = known.name })
	core.emerge_area(vector.subtract(node, REACH), vector.add(node, REACH), function(_, _, left)
		if left ~= 0 then return end
		local now = core.get_player_by_name(name)
		if not now or not travelling[name] then return end -- left meanwhile
		now:set_pos(node)
		core.log("action", "tf_planet: " .. name .. " travelled to " .. known.id .. " at " .. core.pos_to_string(node))
		core.after(HOLD, function()
			local p = core.get_player_by_name(name)
			if p then release(p) end
		end)
	end)
	return true
end

core.register_on_leaveplayer(function(player)
	travelling[player:get_player_name()] = nil
end)

core.register_chatcommand("travel", {
	params = "<body> [<latitude> <longitude>]",
	description = "Go to another body of the world (degrees; its equator when no place is given)",
	privs = { teleport = true },
	func = function(name, param)
		local player = core.get_player_by_name(name)
		if not player then return false, "not in the world" end
		local bodies = core.planet.bodies()
		if not bodies then return false, "this world has no other bodies" end
		local body, lat, lon = param:match("^(%S+)%s*(%S*)%s*(%S*)$")
		if not body then
			local ids = {}
			for _, b in ipairs(bodies) do ids[#ids + 1] = b.id end
			return false, "travel to one of: " .. table.concat(ids, ", ")
		end
		lat, lon = tonumber(lat), tonumber(lon)
		if (lat == nil) ~= (lon == nil) or (lat and math.abs(lat) > 90) then
			return false, "a place is a latitude (−90 to 90) and a longitude, in degrees"
		end
		local ok, why = tf_planet.travel(player, body, lat, lon)
		return ok, ok and ("travelling to " .. body) or why
	end,
})
