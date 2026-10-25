-- Data for the two built-in widgets. The page chooses placement; the server
-- chooses what the player may see and which actions may change the world.

return function(modes, set_mode, damage)

local PERIOD = 2
local EXPLORE_GRID = 256
local EXPLORE_BYTES = EXPLORE_GRID * EXPLORE_GRID / 8
local elapsed = 0
local maps = {} -- player name → explored bitset and last visited cell
local clock_paused = false
local terra = core.terra.enabled()
local world = terra and core.terra.world() or nil

local function can_control(player)
	return core.check_player_privs(player, { server = true })
end

local function send_map(player, explored)
	player:set_ui("map", {
		code = world.code, size = world.size, star = world.star, tilt = world.tilt,
		mode = modes[player:get_player_name()] and "creative" or "survival",
		explored = core.encode_base64(explored), grid = EXPLORE_GRID,
	})
end

local function reveal(mask, cx, cz, radius)
	local changed = false
	for dz = -radius, radius do
		local z = cz + dz
		if z >= 0 and z < EXPLORE_GRID then
			for dx = -radius, radius do
				if dx * dx + dz * dz <= radius * radius then
					local x = (cx + dx) % EXPLORE_GRID
					local bit = z * EXPLORE_GRID + x
					local byte = math.floor(bit / 8) + 1
					local value = mask:byte(byte)
					local power = 2 ^ (bit % 8)
					if math.floor(value / power) % 2 == 0 then
						mask = mask:sub(1, byte - 1) .. string.char(value + power) .. mask:sub(byte + 1)
						changed = true
					end
				end
			end
		end
	end
	return mask, changed
end

local function map_at(player)
	if not terra then return end
	local name = player:get_player_name()
	local state = maps[name]
	local pos = core.terra.world_column(player:get_pos())
	local size = world.size
	local cx = math.floor((pos.x + size / 2) * EXPLORE_GRID / size) % EXPLORE_GRID
	local cz = math.max(0, math.min(EXPLORE_GRID - 1,
		math.floor((size / 2 - pos.z) * EXPLORE_GRID / size)))
	if state.x ~= cx or state.z ~= cz then
		state.x, state.z = cx, cz
		local radius = math.max(2, math.ceil(160 * EXPLORE_GRID / size))
		local changed
		state.explored, changed = reveal(state.explored, cx, cz, radius)
		if changed then
			player:get_meta():set_string("tf_explored", core.encode_base64(state.explored))
			if state.ready and not modes[name] then send_map(player, state.explored) end
		end
	end
	player:set_ui("map_position", { x = pos.x, z = pos.z, date = core.terra.calendar().date })
end

local function controls_for(player)
	if not can_control(player) then
		player:set_ui("world_controls", nil)
		return
	end
	player:set_ui("world_controls", {
		mode = modes[player:get_player_name()] and "creative" or "survival",
		damage = damage,
		time = core.get_timeofday(),
		paused = clock_paused,
	})
end

core.register_on_joinplayer(function(player)
	if terra then
		local stored = player:get_meta():get_string("tf_explored")
		local explored = stored == "" and string.rep("\0", EXPLORE_BYTES) or core.decode_base64(stored)
		if #explored ~= EXPLORE_BYTES then error("tf_player: invalid explored map for " .. player:get_player_name()) end
		maps[player:get_player_name()] = { explored = explored }
		map_at(player)
		send_map(player, maps[player:get_player_name()].explored)
		maps[player:get_player_name()].ready = true
	end
	controls_for(player)
end)

core.register_on_leaveplayer(function(player)
	maps[player:get_player_name()] = nil
end)

core.register_globalstep(function(dtime)
	elapsed = elapsed + dtime
	if elapsed < PERIOD then return end
	elapsed = 0
	for _, player in ipairs(core.get_connected_players()) do
		map_at(player)
		controls_for(player)
	end
end)

local TIMES = { dawn = 0.23, noon = 0.5, dusk = 0.77, midnight = 0 }
core.register_on_ui_event(function(player, name, payload)
	if name ~= "world.mode" and name ~= "world.time" and name ~= "world.clock" then return end
	if not can_control(player) or type(payload) ~= "table" then return end
	if name == "world.mode" then
		if payload.mode ~= "creative" and payload.mode ~= "survival" then return end
		set_mode(player, payload.mode == "creative", true)
		if terra then send_map(player, maps[player:get_player_name()].explored) end
	elseif name == "world.time" then
		local time = TIMES[payload.time]
		if not time then return end
		core.set_timeofday(time)
	elseif name == "world.clock" then
		if type(payload.paused) ~= "boolean" then return end
		clock_paused = payload.paused
		core.set_time_speed(clock_paused and 0 or nil)
	end
	for _, other in ipairs(core.get_connected_players()) do controls_for(other) end
end)

end
