-- An autopilot flies through tf_flight's own space (space.lua) on a live
-- server: up over the blocks to the boundary, into a bubble, burns its
-- orbit clear of the air, coasts it, meets a second bubble and merges, sees
-- something drift off into a bubble of its own, then burns back down and
-- lands on the planet's blocks. Stand-ins for players are plain entities:
-- a bubble carries everything in its cell.
local f = tf_flight
local space = f.space
local T = f.pack.thrust * f.pack.boost
local DT = 0.5
local CELLS = 3 -- the cells this uses: the first ones space opens

local function len(v) return math.sqrt(v.x * v.x + v.y * v.y + v.z * v.z) end
local function scale(v, k) return { x = v.x * k, y = v.y * k, z = v.z * k } end
local function dot(a, b) return a.x * b.x + a.y * b.y + a.z * b.z end
local function add(a, b) return { x = a.x + b.x, y = a.y + b.y, z = a.z + b.z } end
local function say(...) print("FLIGHT " .. table.concat({ ... }, " ")) end

core.register_entity("probe:thing", {
	initial_properties = { physical = false, static_save = false, visual = "sprite", textures = { "blank.png" } },
})

-- the cells' centres and the spawn, kept loaded so the stand-ins stay
local world = core.terra.world()
local function centre(i) return { x = -30000 + 1024 * (i + 0.5), y = 16000, z = world.space + 512 } end
local spawn = { x = 0, y = 200, z = 0 }

local function run()
	-- the map ↔ planet round trip
	local g = core.terra.globe({ x = 1234.25, y = 0, z = -567.5 })
	local b = core.terra.block(g.latitude, g.longitude)
	say("map", string.format("%.3f", b.x), string.format("%.3f", b.z))

	-- over the blocks: up, tipping east, crossing the map no faster than it loads
	local s = { pos = { x = 0, y = 0, z = 0 }, vel = { x = 0, y = 0, z = 0 } }
	local level, t = 0, 0
	while s.pos.y < f.boundary do
		local dir = { x = 0.8, y = 1, z = 0 }
		s = f.near(s, scale(dir, T / len(dir)), 0.1)
		local a = core.terra.across(s.pos.y, s.pos.z)
		level = math.max(level, math.sqrt((s.vel.x / a.east) ^ 2 + (s.vel.z / a.north) ^ 2))
		t = t + 0.1
		if t > 2000 then say("stuck near", s.pos.y) return end
	end
	say("boundary", string.format("%.0f", t), string.format("%.0f", level))

	-- into space: a lone bubble of its own
	local a = core.add_entity(spawn, "probe:thing") or error("FLIGHT no stand-in")
	local bub = space.enter(a, space.up(s))
	if space.at(a:get_pos()) ~= bub then say("not in its bubble") return end

	-- burn to orbit as a pilot flies it: the thrust is the bubble's
	local function burn(dir, dt)
		if dir then
			local fr = core.orbit.frame(bub.state)
			local l = { x = dot(dir, fr.east), y = dot(dir, fr.up), z = dot(dir, fr.north) }
			space.push(bub, scale(l, T / len(l)))
		end
		space.step(dt)
		t = t + dt
		if t > 8000 then error("FLIGHT gave up: " .. core.write_json(core.orbit.elements(bub.state))) end
		return core.orbit.elements(bub.state)
	end
	local e = core.orbit.elements(bub.state)
	while not (e.bound and e.apoapsis > 250000) do
		local d = core.orbit.directions(bub.state)
		e = burn(e.altitude < 50000 and add(d.prograde, d.radial) or d.prograde, DT)
	end
	while core.orbit.where(bub.state).velocity.y > 50 do e = burn(nil, 1) end
	while e.periapsis < 140000 do
		local d = core.orbit.directions(bub.state)
		local k = math.max(-1, math.min(1, -core.orbit.where(bub.state).velocity.y / 50))
		e = burn(add(d.prograde, scale(d.radial, k)), DT)
	end
	say("orbit", string.format("%.0f", e.apoapsis), string.format("%.0f", e.periapsis), string.format("%.0f", e.period))

	-- a whole orbit coasting: still up, and the stand-in still in it
	local around = 0
	while around < e.period do e = burn(nil, 30) around = around + 30 end
	say("coasted", string.format("%.0f", e.periapsis), space.at(a:get_pos()) == bub and "kept" or "lost")
	say("buildable", tostring(space.buildable(bub)))

	-- a second bubble a kilometre east, closing at 10 m/s: they merge
	local fr = core.orbit.frame(bub.state)
	local other = core.add_entity(spawn, "probe:thing") or error("FLIGHT no stand-in")
	local ob = space.enter(other, { pos = add(bub.state.pos, scale(fr.east, 1000)), vel = add(bub.state.vel, scale(fr.east, -10)) })
	if ob == bub then say("joined at once") return end
	local waited = 0
	while space.at(other:get_pos()) ~= bub and waited < 200 do space.step(DT) waited = waited + DT end
	say("merged", space.at(other:get_pos()) == bub and string.format("%.0f", waited) or "never")

	-- the other drifts out past the edge of the shared bubble: a bubble of its own
	other:set_pos(add(other:get_pos(), { x = 0, y = 0, z = 450 }))
	space.step(DT)
	local nb = space.at(other:get_pos())
	say("split", nb and nb ~= bub and "own" or "not")

	-- back down: burn against the motion till the orbit meets the ground, fall
	while core.orbit.elements(bub.state).periapsis > 0 do
		local d = core.orbit.directions(bub.state)
		burn(scale(d.prograde, -1), DT)
	end
	local fell = 0
	while a:get_pos().z >= world.space and fell < 20000 do space.step(1) fell = fell + 1 end
	local p, v = a:get_pos(), a:get_velocity()
	say("down", string.format("%.0f", p.x), string.format("%.0f", p.y - f.sea), string.format("%.0f", len(v)))
	core.request_shutdown()
end

core.after(1, function()
	for i = 0, CELLS - 1 do core.forceload_block(centre(i), true) end
	core.forceload_block(spawn, true)
	core.after(4, run)
end)
