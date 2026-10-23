-- Rock, ore and lava: the blocks terragen's geology places (src/terragen/
-- geology.*). The map generator finds them by alias — mapgen_rock_<rock>,
-- mapgen_ore_<ore>, mapgen_placer, mapgen_lava_source — and refuses to start
-- if one is missing. `_matter` names what each is made of in the engine's
-- matter tables (matter/*.json), for the systems that will melt, smelt and
-- dissolve them.

local function tex(name)
	return "tf_goodvibes_" .. name .. ".png"
end

-- Rocks, by how hard they are to break (cracky 1 hardest … 3 softest).
local ROCKS = {
	{ "granite", "Granite", 2, "granite" },
	{ "gneiss", "Gneiss", 2, "gneiss" },
	{ "slate", "Slate", 3, "slate" },
	{ "basalt", "Basalt", 2, "basalt" },
	{ "andesite", "Andesite", 2, "andesite" },
	{ "tuff", "Tuff", 3, "tuff" },
	{ "obsidian", "Obsidian", 1, "obsidian" },
	{ "shale", "Shale", 3, "shale" },
	{ "limestone", "Limestone", 3, "limestone" },
}
for _, r in ipairs(ROCKS) do
	local id, label, hardness, matter = r[1], r[2], r[3], r[4]
	core.register_node("tf_world:" .. id, {
		description = label,
		tiles = { tex(id) },
		groups = { cracky = hardness, stone = 1 },
		_matter = matter,
	})
	core.register_alias("mapgen_rock_" .. id, "tf_world:" .. id)
end
core.register_alias("mapgen_rock_sandstone", "tf_world:sandstone")

-- Ores: what they are, and the rock-like block they sit in.
local ORES = {
	{ "copper", "Copper ore (chalcopyrite)", "copper_ore", "chalcopyrite", 2 },
	{ "tin", "Tin ore (cassiterite)", "tin_ore", "cassiterite", 2 },
	{ "lead", "Lead ore (galena)", "lead_ore", "galena", 2 },
	{ "gold", "Gold ore", "gold_ore", "gold", 2 },
	{ "iron", "Banded iron ore (hematite)", "iron_ore", "hematite", 2 },
	{ "magnetite", "Magnetite", "magnetite_ore", "magnetite", 2 },
	{ "coal", "Coal", "coal_ore", "coal", 3 },
	{ "salt", "Rock salt", "salt", "salt", 3 },
	{ "sulfur", "Sulfur", "sulfur_ore", "sulfur", 3 },
}
for _, o in ipairs(ORES) do
	local id, label, texture, matter, hardness = o[1], o[2], o[3], o[4], o[5]
	core.register_node("tf_world:ore_" .. id, {
		description = label,
		tiles = { tex(texture) },
		groups = { cracky = hardness, ore = 1 },
		_matter = matter,
	})
	core.register_alias("mapgen_ore_" .. id, "tf_world:ore_" .. id)
end

-- River gravel out of the mountains, with gold and tin washed into it.
core.register_node("tf_world:placer_gravel", {
	description = "Gold-bearing gravel",
	tiles = { tex("placer_gravel") },
	groups = { crumbly = 2, falling_node = 1 },
	_matter = "gravel",
})
core.register_alias("mapgen_placer", "tf_world:placer_gravel")

core.register_node("tf_world:clay", {
	description = "Clay",
	tiles = { tex("clay") },
	groups = { crumbly = 3 },
	_matter = "kaolinite",
})

-- Lava: molten rock. Slow, bright, and it burns.
local lava = {
	description = "Lava",
	drawtype = "liquid",
	tiles = { { name = tex("lava_still"), animation = { type = "vertical_frames", aspect_w = 64, aspect_h = 64, length = 3.0 } } },
	special_tiles = { { name = tex("lava_still"), animation = { type = "vertical_frames", aspect_w = 64, aspect_h = 64, length = 3.0 }, backface_culling = false } },
	paramtype = "light",
	light_source = 13,
	walkable = false,
	pointable = false,
	diggable = false,
	buildable_to = true,
	drowning = 1,
	damage_per_second = 8,
	liquid_alternative_flowing = "tf_world:lava_flowing",
	liquid_alternative_source = "tf_world:lava_source",
	liquid_viscosity = 7,
	liquid_renewable = false,
	liquid_range = 3,
	post_effect_color = { a = 200, r = 255, g = 90, b = 0 },
	groups = { lava = 3, liquid = 2, igniter = 1 },
	_matter = "basalt",
}
local source = table.copy(lava)
source.liquidtype = "source"
core.register_node("tf_world:lava_source", source)

local flowing = table.copy(lava)
flowing.drawtype = "flowingliquid"
flowing.liquidtype = "flowing"
flowing.paramtype2 = "flowingliquid"
flowing.tiles = { tex("lava_flow") }
flowing.special_tiles = {
	{ name = tex("lava_flow"), animation = { type = "vertical_frames", aspect_w = 128, aspect_h = 128, length = 2.0 }, backface_culling = false },
	{ name = tex("lava_flow"), animation = { type = "vertical_frames", aspect_w = 128, aspect_h = 128, length = 2.0 }, backface_culling = true },
}
core.register_node("tf_world:lava_flowing", flowing)
core.register_alias("mapgen_lava_source", "tf_world:lava_source")
