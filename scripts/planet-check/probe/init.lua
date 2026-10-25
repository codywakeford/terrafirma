-- The planet's edges, end to end, on a server (scripts/planet-check.sh):
-- off every side of every face (the middle, and beside both corners),
--   - a walker crosses onto the next face, at the ground's height, still going;
--   - water poured on the edge flows over it onto the next face;
--   - sand dropped past the edge lands on the next face's ground.
-- Prints PLANET lines; the script reads them.

local GROUND = 8 -- flat mapgen's ground level: the top node
local SPEED = 3  -- m/s, the walkers

core.register_entity("planet_probe:walker", {
	initial_properties = {
		physical = true,
		collide_with_objects = false,
		collisionbox = { -0.3, 0, -0.3, 0.3, 1.7, 0.3 },
		visual = "sprite",
		textures = { "blank.png" },
		static_save = false,
	},
})

local results = { pass = 0, fail = 0 }
local function check(ok, what)
	if ok then
		results.pass = results.pass + 1
	else
		results.fail = results.fail + 1
		print("PLANET miss " .. what)
	end
end

local DIRS = { { x = 1, z = 0 }, { x = 0, z = 1 }, { x = -1, z = 0 }, { x = 0, z = -1 } }

-- Every side of every face: the last row's nodes there (the middle, beside the corners).
local function sides()
	local out = {}
	for face = 0, 5 do
		local s = core.planet.square(face)
		for _, d in ipairs(DIRS) do
			for _, i in ipairs({ 1, math.floor(s.width / 2), s.width - 2 }) do
				local x = d.x > 0 and s.x + s.width - 1 or d.x < 0 and s.x or s.x + i
				local z = d.z > 0 and s.z + s.width - 1 or d.z < 0 and s.z or s.z + i
				local edge = { x = x, y = GROUND + 1, z = z }
				local across = core.planet.step(edge, { x = d.x, y = 0, z = d.z })
				out[#out + 1] = { face = face, d = d, edge = edge, across = across,
					name = ("face %d (%d,%d) at %d"):format(face, d.x, d.z, i) }
			end
		end
	end
	return out
end

local function walkers(list, done)
	local going = {}
	for _, s in ipairs(list) do
		local start = { x = s.edge.x - 1.5 * s.d.x, y = GROUND + 0.5, z = s.edge.z - 1.5 * s.d.z }
		local obj = core.add_entity(start, "planet_probe:walker") or error("planet check: no walker")
		obj:set_acceleration({ x = 0, y = -9.81, z = 0 })
		obj:set_velocity({ x = SPEED * s.d.x, y = 0, z = SPEED * s.d.z })
		going[#going + 1] = { s = s, obj = obj }
	end
	core.after(2, function()
		for _, g in ipairs(going) do
			local p, v = g.obj:get_pos(), g.obj:get_velocity()
			local face = core.planet.face(vector.round(p))
			check(face == g.s.across.face, g.s.name .. ": walker on face " .. tostring(face) .. ", not " .. g.s.across.face)
			check(math.abs(p.y - (GROUND + 0.5)) < 0.1, g.s.name .. ": walker at y " .. p.y)
			check(math.sqrt(v.x * v.x + v.z * v.z) > SPEED * 0.9, g.s.name .. ": walker stopped")
			g.obj:remove()
		end
		done()
	end)
end

local function water(list, done)
	-- one side of each edge only: the two sides share nodes (one's edge row is
	-- the other's past-the-edge), and past the edge must start dry
	local key = function(p) return p.x .. "," .. p.z end
	-- (and nowhere near another spring: two side by side make a third between)
	local wet, poured = {}, {}
	for _, s in ipairs(list) do
		if not wet[key(s.edge)] then
			for dz = -3, 3 do
				for dx = -3, 3 do
					wet[key({ x = s.across.pos.x + dx, z = s.across.pos.z + dz })] = true
					wet[key({ x = s.edge.x + dx, z = s.edge.z + dz })] = true
				end
			end
			poured[#poured + 1] = s
		end
	end
	for _, s in ipairs(poured) do
		check(core.get_node(s.across.pos).name == "air", s.name .. ": past the edge isn't dry to start")
		core.set_node(s.edge, { name = "tf_world:water_source" })
	end
	core.after(4, function()
		for _, s in ipairs(poured) do
			local n = core.get_node(s.across.pos).name
			check(n == "tf_world:water_flowing", s.name .. ": past the edge is " .. n)
		end
		for _, s in ipairs(poured) do
			core.set_node(s.edge, { name = "air" })
		end
		done()
	end)
end

local function sand(list, done)
	for _, s in ipairs(list) do
		core.spawn_falling_node({ x = s.across.pos.x, y = GROUND + 6, z = s.across.pos.z })
	end
	core.after(0, function()
		for _, s in ipairs(list) do
			core.set_node({ x = s.across.pos.x, y = GROUND + 6, z = s.across.pos.z }, { name = "tf_world:sand" })
			core.spawn_falling_node({ x = s.across.pos.x, y = GROUND + 6, z = s.across.pos.z })
		end
	end)
	core.after(4, function()
		for _, s in ipairs(list) do
			local n = core.get_node(s.across.pos).name
			check(n == "tf_world:sand", s.name .. ": the sand landed as " .. n)
		end
		done()
	end)
end

core.after(1, function()
	local low, high = core.planet.square(0), core.planet.square(5)
	local minp = { x = low.x - 16, y = 0, z = low.z - 16 }
	local maxp = { x = high.x + high.width + 16, y = 31, z = high.z + high.width + 16 }
	core.emerge_area(minp, maxp, function(_, _, remaining)
		if remaining > 0 then return end
		local list = sides()
		print("PLANET sides " .. #list)
		-- the water first takes the edge rows (the walkers' ground stays), then the
		-- sand on the far side; walkers before either
		walkers(list, function()
			water(list, function()
				sand(list, function()
					print(("PLANET pass %d fail %d"):format(results.pass, results.fail))
					core.request_shutdown()
				end)
			end)
		end)
	end)
end)
