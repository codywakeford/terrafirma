-- A player's body: how the engine moves them, decided in one place from
-- where they are. On the planet they walk under its gravity, which weakens
-- as they climb (R² / (R + h)²: high enough, they hardly fall); riding a
-- craft the craft moves them.

local RETUNE = 0.01 -- the share gravity changes by before the body is told again

local MODES = {
	ground = { speed = 1, jump = 1 },
	riding = { speed = 0, jump = 0, gravity = 0 },
}

-- opts: { sea = y of the sea }
return function(opts)
	local planet = core.orbit.planet()
	local worn = {} -- player name → { mode, gravity } they have

	local function mode(player)
		local parent = player:get_attach()
		if parent and parent:get_luaentity() and parent:get_luaentity().name == "tf_flight:craft" then
			return "riding"
		end
		return "ground"
	end

	-- The share of the ground's gravity at a height (m above the sea).
	local function pull(altitude)
		return (planet.radius / (planet.radius + math.max(altitude, 0))) ^ 2
	end

	-- Bring a player's body in line with where they are (only on a change:
	-- other mods tune speed between, as sprinting does).
	local function settle(player)
		local name = player:get_player_name()
		local m = mode(player)
		local g = m == "ground" and pull(player:get_pos().y - opts.sea) or 0
		local was = worn[name]
		if was and was.mode == m and math.abs(was.gravity - g) <= RETUNE * math.max(g, RETUNE) then return end
		worn[name] = { mode = m, gravity = g }
		local o = table.copy(MODES[m])
		o.gravity = g
		player:set_physics_override(o)
	end

	return {
		settle = settle,
		step = function(player) settle(player) end,
		forget = function(player) worn[player:get_player_name()] = nil end,
		mode = mode,
	}
end
