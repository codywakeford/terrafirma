-- The one scenario this world was built for, centred on the origin. Owns
-- building, rebuilding and where to look from.

local MARGIN = 10   -- floor around the plot: room for spills to settle
local HEADROOM = 8  -- cleared above the plot's own height on a rebuild

return function(id)
	local s = tf_lab.scenarios[id]
	if not s then error("tf_lab: this world is for scenario " .. id .. ", which no mod declares", 0) end

	local size = s.size
	local min = { x = -math.floor(size.x / 2), y = 0, z = -math.floor(size.z / 2) }
	local stage = {
		scenario = s,
		area = tf_lab.Area(min, size),
		values = tf_lab.defaults(s),
		built = false,
	}
	stage.bounds = {
		min = { x = min.x - MARGIN, y = -1, z = min.z - MARGIN },
		max = { x = min.x + size.x - 1 + MARGIN, y = size.y + HEADROOM, z = min.z + size.z - 1 + MARGIN },
	}

	local function clear()
		local b = stage.bounds
		local a = { x = b.min.x, y = 0, z = b.min.z }
		local vm = core.get_voxel_manip(a, b.max)
		local emin, emax = vm:read_from_map(a, b.max)
		local va = VoxelArea(emin, emax)
		local data = vm:get_data()
		for i in va:iterp(a, b.max) do
			data[i] = core.CONTENT_AIR
		end
		vm:set_data(data)
		vm:write_to_map(true)
	end

	function stage.rebuild()
		clear()
		s.behaviour.build(stage.area, stage.values)
	end

	-- Loads the stage, then builds it; done() after.
	function stage.build(done)
		local b = stage.bounds
		core.emerge_area(b.min, b.max, function(_, _, remaining)
			if remaining > 0 then return end
			stage.rebuild()
			stage.built = true
			done()
		end)
	end

	-- Where to stand when stepping in: south of the plot, a little above, facing north.
	function stage.view()
		return {
			pos = { x = min.x + size.x / 2, y = math.ceil(size.y / 2) + 2, z = min.z - 8 },
			yaw = 0,
			pitch = 0.3,
		}
	end

	-- How the browser's camera circles it.
	function stage.orbit(period)
		local own = s.camera or {}
		return {
			center = { x = min.x + size.x / 2, y = size.y / 2, z = min.z + size.z / 2 },
			radius = own.radius or math.max(size.x, size.z) * 0.9 + 6,
			height = own.height or size.y * 0.6 + 4,
			period = period,
		}
	end

	return stage
end
