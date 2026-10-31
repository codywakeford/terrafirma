-- Building materials (sprint 74): what machines and buildings are made
-- of, as against the ground's dirt and stone. Each is a substance (its
-- weight is its matter's density by the share of the block it fills: sawn
-- timber is wood with air in its grain) and looks like what it is. Plain
-- for now: in survival they'll be worked from raw stock (a log sawn, an
-- ingot rolled), not found. `_machine.friction`: how it grips (a tyre's
-- tread is its rim's), where it isn't most things' 0.6.
local function look(base, tint)
	return base .. "^[multiply:" .. tint
end
local metal = "tf_matter_substance.png"

local MATERIALS = {
	{ id = "timber", name = "Timber", matter = "wood", share = 0.4, groups = { choppy = 2 },
		tiles = { "tf_goodvibes_oak_log.png^[transformR90^[colorize:#c49a62:90" } },
	{ id = "aluminium", name = "Aluminium", matter = "aluminium", share = 1, groups = { cracky = 2 },
		tiles = { look(metal, "#d4d8de") }, friction = 0.5 },
	{ id = "steel", name = "Steel", matter = "steel", share = 1, groups = { cracky = 1 },
		tiles = { look(metal, "#7f8a96") }, friction = 0.5 },
	{ id = "rubber", name = "Rubber (a tyre's tread: it grips)", matter = "rubber", share = 1, groups = { choppy = 3 },
		tiles = { look(metal, "#2a2b2e") }, friction = 1.0 },
	{ id = "canvas", name = "Canvas", matter = "wood", share = 0.33, groups = { snappy = 3 },
		tiles = { "tf_goodvibes_sandstone_top.png^[colorize:#ece4cf:200" } },
	{ id = "glass", name = "Glass", matter = "glass", share = 1, groups = { cracky = 3 },
		tiles = { look(metal, "#cfeef4") }, drawtype = "glasslike", alpha = "blend" },
}

for _, m in ipairs(MATERIALS) do
	core.register_node("tf_world:" .. m.id, {
		description = m.name,
		tiles = m.tiles,
		drawtype = m.drawtype,
		use_texture_alpha = m.alpha,
		paramtype = m.drawtype and "light" or nil,
		sunlight_propagates = m.drawtype and true or nil,
		groups = m.groups,
		_matter = { matter = m.matter, share = m.share },
		_machine = m.friction and { friction = m.friction } or nil,
	})
end
