-- A player's body: how the engine moves them. On the planet they walk under
-- its gravity, which weakens as they climb (R² / (R + h)²: high enough, they
-- hardly fall).

local RETUNE = 0.01 -- the share gravity changes by before the body is told again


-- opts: { sea = y of the sea }
return function(opts)
	local planet = core.orbit.planet()
	local worn = {} -- player name → the gravity they have

	-- The share of the ground's gravity at a height (m above the sea).
	local function pull(altitude)
		return (planet.radius / (planet.radius + math.max(altitude, 0))) ^ 2
	end

	-- Bring a player's body in line with where they are (only on a change:
	-- other mods tune speed between, as sprinting does).
	local function settle(player)
		local name = player:get_player_name()
		local g = pull(player:get_pos().y - opts.sea)
		local was = worn[name]
		if was and math.abs(was - g) <= RETUNE * math.max(g, RETUNE) then return end
		worn[name] = g
		player:set_physics_override({ speed = 1, jump = 1, gravity = g })
	end

	return {
		settle = settle,
		step = function(player) settle(player) end,
		forget = function(player) worn[player:get_player_name()] = nil end,
	}
end
