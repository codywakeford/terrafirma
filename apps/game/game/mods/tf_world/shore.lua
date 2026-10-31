-- The water meets the shore (sprint 81). Loose ground beside water is
-- smoothed into slopes, so the top block at the water's edge is partly
-- open; the generator only places water in whole blocks, which left a gap
-- along every beach and bank. Water lies in that open space as a fill
-- (core.matter.set_fill), full to the block's top, asleep like the sea.

local WATER = core.get_content_id("tf_world:water_source")
local SIDES = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } }

local loose = {}
core.register_on_mods_loaded(function()
	for name, def in pairs(core.registered_nodes) do
		if def.groups and (def.groups.loose or 0) > 0 then loose[core.get_content_id(name)] = true end
	end
end)

core.register_on_generated(function(minp, maxp)
	local vm, emin, emax = core.get_mapgen_object("voxelmanip")
	local area = VoxelArea(emin, emax)
	local data = vm:get_data()
	local filled = {}
	for z = minp.z, maxp.z do
		for y = minp.y, maxp.y do
			for x = minp.x, maxp.x do
				local i = area:index(x, y, z)
				-- loose ground, open above (smoothed), with water beside it at its level
				if loose[data[i]] and not loose[data[area:index(x, math.min(y + 1, emax.y), z)]] then
					for _, d in ipairs(SIDES) do
						local nx, nz = x + d[1], z + d[2]
						if area:contains(nx, y, nz) and data[area:index(nx, y, nz)] == WATER then
							filled[#filled + 1] = { x = x, y = y, z = z }
							break
						end
					end
				end
			end
		end
	end
	-- after the generator writes its blocks: a fill is kept beside them
	if #filled > 0 then
		core.after(0, function()
			for _, p in ipairs(filled) do core.matter.set_fill(p, "water", 1) end
		end)
	end
end)
