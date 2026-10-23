-- What fire makes and what makes fire: blocks with no use until the world
-- can burn. Their matter is what they are; the physics does the rest.

local function tex(name, tint)
	local t = "tf_goodvibes_" .. name .. ".png"
	return tint and (t .. "^[multiply:" .. tint) or t
end

-- charcoal: what wood leaves when it burns short of air
core.register_node("tf_matter:charcoal", {
	description = "Charcoal",
	tiles = { tex("gravel", "#3a3633") },
	groups = { crumbly = 2, flammable = 1 },
	_matter = { matter = "carbon", share = 0.15, form = true }, -- light and porous
})

-- fired clay: clay that has been through a kiln
core.register_node("tf_matter:fired_clay", {
	description = "Fired clay",
	tiles = { tex("clay", "#c9764c") },
	groups = { cracky = 3 },
	_matter = { matter = "metakaolin", share = 0.6, form = true },
})

-- kindling: twigs and dry stuff, little enough to catch from a drill
core.register_node("tf_matter:kindling", {
	description = "Kindling",
	drawtype = "plantlike",
	tiles = { tex("dead_bush") },
	inventory_image = tex("dead_bush"),
	paramtype = "light",
	walkable = false,
	sunlight_propagates = true,
	groups = { snappy = 3, flammable = 3 },
	_matter = { matter = "wood", share = 0.01 },
})

-- the light of a flame: air that's burning (never the form air takes; tf_matter sets and clears it)
core.register_node("tf_matter:flame", {
	description = "Flame",
	drawtype = "airlike",
	paramtype = "light",
	light_source = 12,
	sunlight_propagates = true,
	walkable = false,
	pointable = false,
	diggable = false,
	buildable_to = true,
	floodable = true,
	drop = "",
	groups = { not_in_creative_inventory = 1 },
	_matter = { matter = "air" },
})

core.register_craft({ output = "tf_matter:kindling 4", recipe = { { "group:leaves", "group:leaves" } } })
