-- Placeholder textures are flat colours until real art exists.
local function flat(color)
	return "[fill:16x16:" .. color
end

core.register_item(":", {
	type = "none",
	wield_image = "blank.png",
	tool_capabilities = {
		full_punch_interval = 0.9,
		max_drop_level = 0,
		groupcaps = {
			crumbly = { times = { [1] = 3.0, [2] = 1.5, [3] = 0.6 }, uses = 0 },
			choppy = { times = { [1] = 4.0, [2] = 2.5, [3] = 1.2 }, uses = 0 },
			cracky = { times = { [3] = 6.0 }, uses = 0 },
		},
		damage_groups = { fleshy = 1 },
	},
})

core.register_node("tf_world:stone", {
	description = "Stone",
	tiles = { flat("#7a7a7a") },
	groups = { cracky = 3 },
})

core.register_node("tf_world:dirt", {
	description = "Dirt",
	tiles = { flat("#6b4a2b") },
	groups = { crumbly = 3 },
})

core.register_node("tf_world:grass", {
	description = "Grass",
	tiles = { flat("#4f8a2f"), flat("#6b4a2b"), flat("#5a6b30") },
	groups = { crumbly = 3 },
	drop = "tf_world:dirt",
})

core.register_node("tf_world:sand", {
	description = "Sand",
	tiles = { flat("#d8c88a") },
	groups = { crumbly = 3, falling_node = 1 },
})

local water = {
	description = "Water",
	drawtype = "liquid",
	tiles = { flat("#2b5fa8b0") },
	use_texture_alpha = "blend",
	paramtype = "light",
	walkable = false,
	pointable = false,
	diggable = false,
	buildable_to = true,
	drowning = 1,
	liquid_alternative_flowing = "tf_world:water_flowing",
	liquid_alternative_source = "tf_world:water_source",
	liquid_viscosity = 1,
	post_effect_color = { a = 110, r = 30, g = 70, b = 140 },
	groups = { water = 3, liquid = 3 },
}

core.register_node("tf_world:water_source", table.copy(water))

local flowing = table.copy(water)
flowing.drawtype = "flowingliquid"
flowing.special_tiles = { { name = flat("#2b5fa8b0"), backface_culling = false },
	{ name = flat("#2b5fa8b0"), backface_culling = true } }
flowing.paramtype2 = "flowingliquid"
core.register_node("tf_world:water_flowing", flowing)

-- The engine's built-in mapgens place these by alias.
core.register_alias("mapgen_stone", "tf_world:stone")
core.register_alias("mapgen_water_source", "tf_world:water_source")
core.register_alias("mapgen_river_water_source", "tf_world:water_source")

core.register_biome({
	name = "grassland",
	node_top = "tf_world:grass",
	depth_top = 1,
	node_filler = "tf_world:dirt",
	depth_filler = 3,
	y_min = 3,
	y_max = 31000,
	heat_point = 50,
	humidity_point = 50,
})

core.register_biome({
	name = "shore",
	node_top = "tf_world:sand",
	depth_top = 1,
	node_filler = "tf_world:sand",
	depth_filler = 3,
	y_min = -31000,
	y_max = 2,
	heat_point = 50,
	humidity_point = 50,
})
