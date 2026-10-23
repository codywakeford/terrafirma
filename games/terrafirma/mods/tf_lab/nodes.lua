-- The Lab's floor: a neutral tile, and a slightly lighter pad under each
-- plot. Registered in every world (registration happens at load), used only
-- in the Lab.

local function tile(base, line)
	return "[fill:16x16:" .. line .. "^[fill:15x15:1,1:" .. base
end

core.register_node("tf_lab:floor", {
	description = "Lab floor",
	tiles = { tile("#3b4046", "#2e3237") },
	groups = { cracky = 3, not_in_creative_inventory = 1 },
})

core.register_node("tf_lab:pad", {
	description = "Lab plot",
	tiles = { tile("#525960", "#3f454b") },
	groups = { cracky = 3, not_in_creative_inventory = 1 },
})
