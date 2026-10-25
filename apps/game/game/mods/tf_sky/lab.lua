-- tf_sky's Lab scenario (data in lab.json): the solar sky for a chosen
-- latitude, day and tilt — the same formulas as terragen's sun.

tf_lab.load()

local function solar(values)
	local declination = values.tilt * math.sin(2 * math.pi * values.day / 365)
	return { latitude = values.latitude, declination = declination }
end

local function apply(player, values)
	player:set_sky({ solar = solar(values) })
end

local TIMES = { noon = 0.5, midnight = 0.0 }

tf_lab.behaviour("sky.sun", {
	build = function(area)
		area:fill({ x = 0, y = 0, z = 0 }, { x = 8, y = 0, z = 8 }, "tf_world:snow")
		local c = area:centre()
		area:fill({ x = c.x, y = 1, z = c.z }, { x = c.x, y = 5, z = c.z }, "tf_world:stone")
		area:set({ x = c.x, y = 1, z = 8 }, "tf_world:log") -- marks north
	end,
	enter = apply,
	control = function(id, value, ctx)
		if TIMES[id] then
			core.set_timeofday(TIMES[id])
			return
		end
		for _, player in ipairs(ctx.players()) do apply(player, ctx.values) end
	end,
})
