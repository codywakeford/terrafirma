-- tf_world's Lab scenarios (data in lab.json): the engine's liquids and
-- falling blocks.

local WATER = { ["tf_world:water_source"] = true, ["tf_world:water_flowing"] = true }

tf_lab.load()

tf_lab.behaviour("water.spill", {
	build = function(area, values)
		area:fill({ x = 0, y = 0, z = 0 }, { x = 6, y = 1, z = 6 }, "tf_world:stone")
		area:fill({ x = 1, y = 0, z = 1 }, { x = 5, y = 1, z = 5 }, "tf_world:water_source")
		-- the gap, facing south (where the camera starts), centred
		local left = 3 - math.floor((values.gap - 1) / 2)
		area:fill({ x = left, y = 0, z = 0 }, { x = left + values.gap - 1, y = 1, z = 0 }, "air")
	end,
	expect = {
		after = 5,
		check = function(area)
			local outside = area:get({ x = 3, y = 0, z = -2 })
			return WATER[outside], "two blocks past the gap is " .. outside
		end,
	},
})

tf_lab.behaviour("falling.blocks", {
	build = function(area, values)
		local c = area:centre()
		area:fill({ x = c.x, y = 4, z = c.z }, { x = c.x, y = 6, z = c.z }, values.material)
		area:set({ x = c.x - 1, y = 5, z = c.z }, "tf_world:stone") -- stays: stone doesn't fall
		core.check_for_falling(area:pos({ x = c.x, y = 4, z = c.z }))
	end,
	expect = {
		after = 4,
		check = function(area, values)
			local c = area:centre()
			local bottom, top = area:get({ x = c.x, y = 0, z = c.z }), area:get({ x = c.x, y = 6, z = c.z })
			local stone = area:get({ x = c.x - 1, y = 5, z = c.z })
			return bottom == values.material and top == "air" and stone == "tf_world:stone",
				"floor " .. bottom .. ", top " .. top .. ", stone " .. stone
		end,
	},
})
