-- Flight on a planet, end to end (scripts/flight-check.sh): the planet's own
-- physics (core.orbit on its radius, core.planet.fly through the map) takes a
-- craft up from the ground, round the planet once in orbit over every face of
-- the equator, and back down. Prints FLIGHT lines; the script reads them.

local CRAFT = { mass = 100, drag_area = 0.7 } -- tf_flight's pack
local DT = 0.1

local function say(...) print("FLIGHT " .. table.concat({ ... }, " ")) end

core.after(1, function()
	local planet = core.orbit.planet()
	say("planet", math.floor(planet.radius), ("%.3f"):format(core.orbit.density(0)), ("%.6f"):format(core.orbit.density(2500)))
	local sea = tonumber(core.get_mapgen_setting("water_level"))
	local s = core.planet.square(0)
	local pos = { x = s.x + s.width / 2, y = sea + 200, z = s.z + s.width / 2 }
	local vel = { x = 0, y = 0, z = 0 }
	local function fly(thrust, seconds)
		for _ = 1, math.floor(seconds / DT) do
			local r = core.planet.fly(pos, vel, CRAFT, thrust, DT)
			pos, vel = r.pos, r.vel
		end
	end

	-- up to 2.5 km, held there by the climb (a height controller), while
	-- burning east to orbital speed over the turning ground
	local orbital = math.sqrt(planet.gravity * planet.radius ^ 2 / (planet.radius + 2500))
	local ground = orbital - 2 * math.pi / planet.day * (planet.radius + 2500)
	for _ = 1, 3000 do
		local height = pos.y - sea
		if vel.x >= ground and math.abs(height - 2500) < 20 and math.abs(vel.y) < 1 then break end
		-- this high on a small planet gravity is a fraction of the ground's,
		-- and going round lifts: the push up (or down) that holds the height
		local r = planet.radius + height
		local hold = planet.gravity * (planet.radius / r) ^ 2 - (vel.x + 2 * math.pi / planet.day * r) ^ 2 / r
		local up = math.max(-4 * planet.gravity, math.min(4 * planet.gravity, hold + 0.02 * (2500 - height) - 0.6 * vel.y))
		local east = height > 1500 and vel.x < ground and 2 * planet.gravity or 0
		fly({ x = east * CRAFT.mass, y = up * CRAFT.mass, z = 0 }, DT)
	end
	vel.y = 0
	say("climbed", math.floor(pos.y - sea), math.floor(vel.x))

	-- round once, coasting: the height holds, the four faces of the equator pass under it
	local faces, last, low, high = {}, nil, math.huge, -math.huge
	local period = 2 * math.pi * (planet.radius + 2500) / orbital
	for _ = 1, math.floor(period / DT) do
		fly({ x = 0, y = 0, z = 0 }, DT)
		local f = core.planet.face(vector.round(pos))
		if f and f ~= last then faces[#faces + 1] = f; last = f end
		low, high = math.min(low, pos.y - sea), math.max(high, pos.y - sea)
	end
	say("orbit", table.concat(faces, ","), math.floor(low), math.floor(high))

	-- down: burn against the motion until it stands still over the ground, then fall
	for _ = 1, 600 do
		if vel.x <= 0 then break end
		fly({ x = -2 * planet.gravity * CRAFT.mass, y = 0, z = 0 }, DT)
	end
	for _ = 1, 6000 do
		fly({ x = 0, y = 0, z = 0 }, DT)
		local top = core.terra.surface(vector.round(pos))
		if pos.y <= math.max(top.ground, top.water or top.ground) + 1 then break end
	end
	local top = core.terra.surface(vector.round(pos))
	say("landed", math.floor(pos.y - math.max(top.ground, top.water or top.ground)))
	core.request_shutdown()
end)
