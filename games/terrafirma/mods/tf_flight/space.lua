-- Space: what's above the boundary, as places to be.
--
-- Up to the boundary the map is the planet: a block is a metre and the
-- planet's gravity pulls. Above it, everything rides in a bubble: a cell of
-- the map, past the polar wall (core.terra.world().space), whose centre
-- falls around the planet on an orbit (core.orbit). Inside a cell it's
-- ordinary map: you float about, build, walk a station's floor. Everything
-- in a cell shares its orbit, so they hardly move apart, as in a real one.
--
-- - Crossing the boundary going up puts you in a bubble on the orbit you
--   were on (a lone one of your own, or one close enough to join).
-- - Two bubbles that come within JOIN merge: the lone one's contents move
--   into the other, where they are relative to it.
-- - Something LEAVE from a shared or built bubble's centre gets a bubble of
--   its own; a lone bubble follows what's in it instead.
-- - A bubble falling BAND below the boundary puts what's in it back over the
--   block under where it now is.
-- - Built bubbles (a station: blocks placed in its cell) hold their orbit:
--   blocks can't re-enter until vehicles can carry them, so building needs
--   an orbit that stays above the boundary.

local V = dofile(core.get_modpath("tf_flight") .. "/vec.lua")

local CELL = 1024     -- blocks along each side of a cell
local HEIGHT = 16000  -- y of the cells' centres: high above where anything else is drawn
local EDGE = 30000    -- |x| the cells keep within (the map ends at 31000)
local LEAVE = 400     -- m from the centre where something leaves its bubble
local JOIN = 300      -- m between bubbles where a lone one joins the other
local BAND = 200      -- m below the boundary a bubble comes down at (no flicker across it)
local STATION = { mass = 1e7, drag_area = 0 } -- a built bubble: holds its orbit

-- opts: { boundary = m above the sea, sea = y of the sea, space = z where
-- space begins, lone = craft of a bubble with no blocks, store = mod storage }
return function(opts)
	local bubbles = {} -- id → { id, cell, state, built }; runtime: members
	local byCell = {}  -- cell → bubble
	local pushes = {}  -- id → { thrust = local N, warp } this step
	local nextId = 1
	local cols = math.floor(2 * EDGE / CELL)
	local rows = math.floor((EDGE - opts.space) / CELL)

	local cells = {
		centre = function(i)
			local col, row = i % cols, math.floor(i / cols)
			return { x = -EDGE + CELL * (col + 0.5), y = HEIGHT, z = opts.space + CELL * (row + 0.5) }
		end,
		of = function(pos)
			if pos.z < opts.space or math.abs(pos.y - HEIGHT) > CELL / 2 then return nil end
			local col, row = math.floor((pos.x + EDGE) / CELL), math.floor((pos.z - opts.space) / CELL)
			if col < 0 or col >= cols or row < 0 or row >= rows then return nil end
			return row * cols + col
		end,
	}

	local function save()
		local out = {}
		for id, b in pairs(bubbles) do out[#out + 1] = { id = id, cell = b.cell, state = b.state, built = b.built } end
		opts.store:set_string("bubbles", core.serialize({ next = nextId, bubbles = out }))
	end

	local function load()
		local saved = opts.store:get_string("bubbles")
		if saved == "" then return end
		local t = core.deserialize(saved) or error("tf_flight: the saved bubbles can't be read")
		nextId = t.next
		for _, b in ipairs(t.bubbles) do
			bubbles[b.id] = { id = b.id, cell = b.cell, state = b.state, built = b.built, members = {} }
			byCell[b.cell] = bubbles[b.id]
		end
	end

	-- ---- bubbles ------------------------------------------------------------

	local function open(state)
		for i = 0, cols * rows - 1 do
			if not byCell[i] then
				local b = { id = nextId, cell = i, state = state, built = false, members = {} }
				nextId = nextId + 1
				bubbles[b.id], byCell[i] = b, b
				local c = cells.centre(i)
				core.emerge_area(vector.offset(c, -32, -32, -32), vector.offset(c, 32, 32, 32))
				save()
				return b
			end
		end
		error("tf_flight: every cell of space is taken")
	end

	local function free(b)
		bubbles[b.id], byCell[b.cell], pushes[b.id] = nil, nil, nil
		save()
	end

	-- Every bubble, oldest first: a snapshot, so a step can open and free them.
	local function all()
		local out = {}
		for _, b in pairs(bubbles) do out[#out + 1] = b end
		table.sort(out, function(x, y) return x.id < y.id end)
		return out
	end

	local function at(pos)
		local i = cells.of(pos)
		return i and byCell[i]
	end

	-- What's in a bubble: everything in its cell that isn't riding on something else.
	local function members(b)
		local c = cells.centre(b.cell)
		local out = {}
		for obj in core.objects_in_area(vector.offset(c, -CELL / 2, -CELL / 2, -CELL / 2), vector.offset(c, CELL / 2, CELL / 2, CELL / 2)) do
			if not obj:get_attach() then out[#out + 1] = obj end
		end
		return out
	end

	-- The orbit of something at `pos` in a bubble's cell moving at `vel` in it, and back.
	local function locate(b, pos, vel)
		local f = core.orbit.frame(b.state)
		local d = V.sub(pos, cells.centre(b.cell))
		return { pos = V.add(b.state.pos, V.out(f, d)), vel = V.add(b.state.vel, V.out(f, vel)) }
	end
	local function place(b, state)
		local f = core.orbit.frame(b.state)
		return V.add(cells.centre(b.cell), V.into(f, V.sub(state.pos, b.state.pos))), V.into(f, V.sub(state.vel, b.state.vel))
	end

	local function velocity(obj)
		return obj:get_velocity() or V.zero()
	end
	local function move(obj, pos, vel)
		obj:set_pos(pos)
		if obj:is_player() then obj:add_velocity(V.sub(vel, velocity(obj))) else obj:set_velocity(vel) end
	end

	-- ---- the boundary -------------------------------------------------------

	-- From the map (pos: map x, z and m up from the sea; vel: true m/s east,
	-- up and north) to an orbit over where it is, and back to the point of
	-- the map under an orbit.
	local function up(near)
		local g = core.terra.globe(near.pos)
		return core.orbit.at({ latitude = g.latitude, longitude = g.longitude, altitude = near.pos.y, velocity = near.vel })
	end
	local function down(state)
		local w = core.orbit.where(state)
		local b = core.terra.block(w.latitude, w.longitude)
		return { pos = { x = b.x, y = w.altitude, z = b.z }, vel = w.velocity }
	end

	-- Into space, on an orbit: the bubble it joins, or a lone one of its own.
	local function enter(obj, state)
		local b
		for _, o in pairs(bubbles) do
			if V.length(V.sub(o.state.pos, state.pos)) < JOIN then b = o break end
		end
		b = b or open(state)
		local pos, vel = place(b, state)
		move(obj, pos, vel)
		b.members[#b.members + 1] = obj
		return b
	end

	-- Out of a bubble, back over the planet's blocks.
	local function land(b, obj)
		local n = down(locate(b, obj:get_pos(), velocity(obj)))
		local pos = { x = n.pos.x, y = n.pos.y + opts.sea, z = n.pos.z }
		local e = obj:get_luaentity()
		if e and e.land then
			e:land(n, pos) -- a craft takes its rider down with it
		else
			local a = core.terra.across(n.pos.y, n.pos.z) -- the engine moves it in blocks
			move(obj, pos, { x = n.vel.x / a.east, y = n.vel.y, z = n.vel.z / a.north })
		end
		core.emerge_area(vector.offset(vector.round(pos), -32, -opts.boundary - 64, -32), vector.offset(vector.round(pos), 32, 0, 32))
	end

	-- ---- each step ----------------------------------------------------------

	local function lone(b) return not b.built and #b.members == 1 end

	-- A lone bubble's contents join one that's come close: a built one, or
	-- an older one (so of two lone bubbles meeting, one stays).
	local function merge(b)
		for _, o in pairs(bubbles) do
			if o ~= b and (o.built or o.id < b.id) and V.length(V.sub(o.state.pos, b.state.pos)) < JOIN then
				for _, obj in ipairs(b.members) do
					local pos, vel = place(o, locate(b, obj:get_pos(), velocity(obj)))
					move(obj, pos, vel)
					o.members[#o.members + 1] = obj
				end
				free(b)
				return true
			end
		end
	end

	-- What's drifted from the centre: its own bubble, or a lone bubble follows it.
	local function spread(b)
		local c = cells.centre(b.cell)
		local kept = {}
		for _, obj in ipairs(b.members) do
			local pos = obj:get_pos()
			if V.length(V.sub(pos, c)) <= LEAVE then
				kept[#kept + 1] = obj
			elseif lone(b) then
				b.state = locate(b, pos, velocity(obj))
				move(obj, c, V.zero())
				kept[#kept + 1] = obj
			else
				local nb = open(locate(b, pos, velocity(obj)))
				move(obj, cells.centre(nb.cell), V.zero())
				nb.members = { obj }
			end
		end
		b.members = kept
	end

	local function fly(b, dt)
		local p = pushes[b.id]
		local f = core.orbit.frame(b.state)
		local thrust = p and lone(b) and V.out(f, p.thrust) or V.zero()
		local warp = p and lone(b) and p.warp or 1
		b.state = core.orbit.step(b.state, b.built and STATION or opts.lone, thrust, dt * warp)
		local w = core.orbit.where(b.state)
		if w.altitude < opts.boundary - BAND and w.velocity.y < 0 then
			if b.built then error("tf_flight: a built bubble fell to the boundary (its orbit should hold)") end
			for _, obj in ipairs(b.members) do land(b, obj) end
			free(b)
		end
	end

	load()

	return {
		boundary = opts.boundary,
		up = up,
		down = down,
		enter = enter,
		at = at,
		locate = function(b, obj) return locate(b, obj:get_pos(), velocity(obj)) end,
		lone = function(b, obj) return lone(b) and b.members[1] == obj end,
		-- Thrust (N, local: x east, y up, z north) on a lone bubble this step;
		-- time runs `warp` times faster for it.
		push = function(b, thrust, warp) pushes[b.id] = { thrust = thrust, warp = warp or 1 } end,
		-- Whether blocks can go in a bubble's cell: built bubbles keep their
		-- orbit, so its lowest point must stay above the boundary.
		buildable = function(b)
			return b.built or core.orbit.elements(b.state).periapsis > opts.boundary
		end,
		build = function(b)
			if not b.built then b.built = true save() end
		end,
		step = function(dt)
			for _, b in ipairs(all()) do
				b.members = members(b)
				if #b.members == 0 and not b.built then free(b) end
			end
			for _, b in ipairs(all()) do
				if bubbles[b.id] == b and lone(b) then merge(b) end
			end
			for _, b in ipairs(all()) do spread(b) end
			for _, b in ipairs(all()) do
				if bubbles[b.id] == b then fly(b, dt) end
			end
			pushes = {}
		end,
		save = save,
	}
end
