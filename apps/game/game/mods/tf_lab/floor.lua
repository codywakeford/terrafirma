-- Lays the floor as the (otherwise empty) map generates: tile at y = -1
-- inside the stage's bounds, a lighter pad under the plot itself.

return function(stage)
	local function on_pad(x, z)
		local min, size = stage.area.min, stage.area.size
		return x >= min.x and x < min.x + size.x and z >= min.z and z < min.z + size.z
	end

	core.register_on_generated(function(minp, maxp)
		local b = stage.bounds
		if minp.y > -1 or maxp.y < -1 or maxp.x < b.min.x or minp.x > b.max.x
				or maxp.z < b.min.z or minp.z > b.max.z then
			return
		end
		local floor, pad = core.get_content_id("tf_lab:floor"), core.get_content_id("tf_lab:pad")
		local vm, emin, emax = core.get_mapgen_object("voxelmanip")
		local va = VoxelArea(emin, emax)
		local data = vm:get_data()
		for z = math.max(minp.z, b.min.z), math.min(maxp.z, b.max.z) do
			for x = math.max(minp.x, b.min.x), math.min(maxp.x, b.max.x) do
				data[va:index(x, -1, z)] = on_pad(x, z) and pad or floor
			end
		end
		vm:set_data(data)
		vm:write_to_map()
	end)
end
