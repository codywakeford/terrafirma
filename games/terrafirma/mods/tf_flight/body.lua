-- A player's body: how the engine moves them, decided in one place from
-- where they are. On the planet they walk under gravity; riding a craft the
-- craft moves them; in space (a bubble's cell) they float: the same keys walk
-- them about, Space and Shift up and down, nothing pulls them down.

local PUFF = 4 -- m/s² floating: Space up, Shift down (the walk keys move you across)

local MODES = {
	ground = { speed = 1, jump = 1, gravity = 1 },
	riding = { speed = 0, jump = 0, gravity = 0 },
	floating = { speed = 1, jump = 0, gravity = 0 },
}

-- opts: { space = z north of which the map is space }
return function(opts)
	local worn = {} -- player name → the mode they're in

	local function mode(player)
		local parent = player:get_attach()
		if parent and parent:get_luaentity() and parent:get_luaentity().name == "tf_flight:craft" then
			return "riding"
		end
		return player:get_pos().z >= opts.space and "floating" or "ground"
	end

	-- Bring a player's body in line with where they are (only on a change:
	-- other mods tune speed between, as sprinting does).
	local function settle(player)
		local name = player:get_player_name()
		local m = mode(player)
		if worn[name] == m then return end
		worn[name] = m
		player:set_physics_override(MODES[m])
	end

	return {
		settle = settle,
		-- Each step: settled, and floating, Space and Shift move them up and down.
		step = function(player, dtime)
			settle(player)
			if worn[player:get_player_name()] ~= "floating" then return end
			local c = player:get_player_control()
			local up = (c.jump and 1 or 0) - (c.sneak and 1 or 0)
			if up ~= 0 then player:add_velocity({ x = 0, y = up * PUFF * dtime, z = 0 }) end
		end,
		forget = function(player) worn[player:get_player_name()] = nil end,
		mode = mode,
	}
end
