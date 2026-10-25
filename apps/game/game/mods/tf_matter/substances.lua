-- A block of every substance the engine knows, so anything can be held,
-- placed, heated and mixed: its solid, and its liquid where it has one at
-- 1 atm. Each is the form its matter takes in that state, after the world's
-- own blocks (lava still cools into basalt, water still freezes into ice;
-- molten copper that cools becomes a copper block). The catalogue shows each
-- substance as it is at room temperature; the rest exist for the physics.

local LOOKS = { -- what the common ones look like; the rest get a colour of their own
	water = "#3d7fd6", wood = "#8a5a32", carbon = "#2a2a2c", iron = "#8c8f94", copper = "#c8743c", tin = "#c4c8cc",
	gold = "#e6c048", lead = "#6d7178", aluminium = "#c9ccd2", zinc = "#a9b0b8", bronze = "#b4823c", cast_iron = "#5f6266",
	steel = "#9aa0a8", pewter = "#a6a9ad", glass = "#bfe3ea", silica = "#e6dcc4", salt = "#f2f2f2", sulfur = "#e8d03a",
	lime = "#f0eee6", slaked_lime = "#ecebe2", calcite = "#e8e2d0", soda_ash = "#f4f4f0", hematite = "#8a3a2a",
	magnetite = "#3a3a3e", malachite = "#2e8a5a", cassiterite = "#5a4a3e", galena = "#6a6e78", glucose = "#f6f2e6",
	kaolinite = "#d8cdb8", metakaolin = "#c9a98a", mercury = "#b8bcc4", sulfuric_acid = "#d8d4a0", silicon = "#6a7080",
	carbon_dioxide = "#e8eef2", ammonia = "#e4ecec", methane = "#dfe6ea", nitrogen = "#dde6f0", oxygen = "#cfe0f4",
	hydrogen = "#e8f0f8", carbon_monoxide = "#e2e6ea", sulfur_dioxide = "#eeeadc",
}

local function colour(id)
	if LOOKS[id] then return LOOKS[id] end
	local h = 0
	for i = 1, #id do h = (h * 31 + id:byte(i)) % 360 end
	-- a muted mineral tone from the id
	local r, g, b = 140 + (h % 60), 120 + ((h * 7) % 60), 100 + ((h * 13) % 60)
	return string.format("#%02x%02x%02x", r, g, b)
end

-- groups, hidden from the catalogue unless `shown` (a group that's present at all hides it)
local function groups(shown)
	local g = { cracky = 3, creative_matter = 1 }
	if not shown then g.not_in_creative_inventory = 1 end
	return g
end

for _, s in ipairs(core.matter.substances()) do
	local tint = colour(s.id)
	local solid_shown = s.state == "solid"
	-- the solid: every substance has one (frozen nitrogen included); a gas or liquid at room temperature hides it
	core.register_node("tf_matter:" .. s.id, {
		description = s.name .. " (" .. s.formula .. ")",
		tiles = { "tf_matter_substance.png^[multiply:" .. tint },
		groups = groups(solid_shown),
		_matter = { matter = s.id, share = 1, form = true },
		_matter_generated = true,
	})
	-- the liquid, where it has one at 1 atm: still (heat-made liquids don't flow yet), bright when hot
	local liquid = s.boils == nil or s.boils > s.melts
	if liquid and s.melts < 3500 then
		local hot = s.melts > 700
		core.register_node("tf_matter:" .. s.id .. "_liquid", {
			description = "Liquid " .. s.name:lower() .. " (" .. s.formula .. ")",
			drawtype = "glasslike",
			tiles = { "tf_matter_liquid.png^[multiply:" .. tint },
			use_texture_alpha = "blend",
			paramtype = "light",
			light_source = hot and 10 or 0,
			walkable = false,
			pointable = true,
			buildable_to = false,
			drowning = 1,
			damage_per_second = hot and 6 or 0,
			post_effect_color = { a = 160, r = 120, g = 120, b = 130 },
			groups = groups(s.state == "liquid"),
			_matter = { matter = s.id, share = 1, state = "liquid", form = true },
			_matter_generated = true,
		})
	end
end
