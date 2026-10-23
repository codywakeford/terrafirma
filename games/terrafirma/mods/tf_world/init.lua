local water_texture = "tf_goodvibes_water_still.png"
local water_animation = { type = "vertical_frames", aspect_w = 512, aspect_h = 512, length = 4.0 }

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
			snappy = { times = { [3] = 0.4 }, uses = 0 },
		},
		damage_groups = { fleshy = 1 },
	},
})

core.register_node("tf_world:stone", {
	description = "Stone",
	tiles = { "tf_goodvibes_stone.png" },
	groups = { cracky = 3 },
})

core.register_node("tf_world:dirt", {
	description = "Dirt",
	tiles = { "tf_goodvibes_dirt.png" },
	groups = { crumbly = 3 },
})

core.register_node("tf_world:grass", {
	description = "Grass",
	tiles = { "tf_goodvibes_grass_block_top.png", "tf_goodvibes_dirt.png",
		"tf_goodvibes_grass_block_side.png" },
	groups = { crumbly = 3 },
	drop = "tf_world:dirt",
})

core.register_node("tf_world:sand", {
	description = "Sand",
	tiles = { "tf_goodvibes_sand.png" },
	groups = { crumbly = 3, falling_node = 1 },
})

local water = {
	description = "Water",
	drawtype = "liquid",
	tiles = { { name = water_texture, animation = water_animation } },
	special_tiles = { { name = water_texture, animation = water_animation, backface_culling = false } },
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

local source = table.copy(water)
source.liquidtype = "source"
core.register_node("tf_world:water_source", source)

local flowing = table.copy(water)
flowing.drawtype = "flowingliquid"
flowing.special_tiles = { { name = water_texture, animation = water_animation, backface_culling = false },
	{ name = water_texture, animation = water_animation, backface_culling = true } }
flowing.paramtype2 = "flowingliquid"
flowing.liquidtype = "flowing"
core.register_node("tf_world:water_flowing", flowing)

-- The engine's built-in mapgens place these by alias.
core.register_alias("mapgen_stone", "tf_world:stone")
core.register_alias("mapgen_water_source", "tf_world:water_source")
core.register_alias("mapgen_river_water_source", "tf_world:water_source")

assert(loadfile(core.get_modpath("tf_world") .. "/geology.lua"))()
assert(loadfile(core.get_modpath("tf_world") .. "/biomes.lua"))()
assert(loadfile(core.get_modpath("tf_world") .. "/vegetation.lua"))()

-- Placeholder starter kit until survival progression exists: anyone joining
-- with an empty inventory gets a few of each block, so the HUD has something to show.
core.register_on_joinplayer(function(player)
	local inv = player:get_inventory()
	if not inv:is_empty("main") then
		return
	end
	for _, name in ipairs({ "tf_world:grass", "tf_world:dirt", "tf_world:stone", "tf_world:sand" }) do
		inv:add_item("main", name .. " 64")
	end
end)

-- Placeholder crafts so the inventory's crafting grid has something to make.
core.register_node("tf_world:sandstone", {
	description = "Sandstone",
	tiles = { "tf_goodvibes_sandstone_top.png", "tf_goodvibes_sandstone_bottom.png",
		"tf_goodvibes_sandstone.png" },
	groups = { cracky = 3 },
})

core.register_node("tf_world:stone_bricks", {
	description = "Stone Bricks",
	tiles = { "tf_goodvibes_stone_bricks.png" },
	groups = { cracky = 3 },
})

core.register_craft({
	output = "tf_world:sandstone",
	recipe = { { "tf_world:sand", "tf_world:sand" }, { "tf_world:sand", "tf_world:sand" } },
})

core.register_craft({
	output = "tf_world:stone_bricks 4",
	recipe = { { "tf_world:stone", "tf_world:stone" }, { "tf_world:stone", "tf_world:stone" } },
})
dofile(core.get_modpath("tf_world") .. "/lab.lua")
