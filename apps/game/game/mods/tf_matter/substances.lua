-- A block of every substance the engine knows, so anything can be held,
-- placed, heated and mixed: its solid, and its liquid where it has one at
-- 1 atm. Each is the form its matter takes in that state, after the world's
-- own blocks (lava still cools into basalt, water still freezes into ice;
-- molten copper that cools becomes a copper block). The catalogue shows each
-- substance as it is at room temperature; the rest exist for the physics.

-- how each looks is the matter table's (sprint 79: substances.json's colour)
-- groups, hidden from the catalogue unless `shown` (a group that's present at all hides it)
local function groups(shown)
	local g = { cracky = 3, creative_matter = 1 }
	if not shown then g.not_in_creative_inventory = 1 end
	return g
end

-- a block's weight, for its description (sprint 80: what's carried weighs something)
local function weight(kg)
	if kg >= 1000 then return string.format("%.2f t", kg / 1000) end
	return string.format("%d kg", math.floor(kg + 0.5))
end

-- a levelled block: param2 says how full it is, in sixty-fourths (sprint 80)
local LEVELLED = { type = "leveled", fixed = { -0.5, -0.5, -0.5, 0.5, 0.5, 0.5 } }

for _, s in ipairs(core.matter.substances()) do
	local tint = s.colour
	local solid_shown = s.state == "solid"
	-- the solid: every substance has one (frozen nitrogen included); a gas or liquid at room temperature hides it
	core.register_node("tf_matter:" .. s.id, {
		description = s.name .. " (" .. s.formula .. ")\n1 m³ · " .. weight(s.density),
		tiles = { "tf_matter_substance.png^[multiply:" .. tint },
		groups = groups(solid_shown),
		_matter = { matter = s.id, share = 1, form = true },
		_matter_generated = true,
	})
	-- its powder: poured, it heaps and slides (sprint 80); levelled by how much is there
	core.register_node("tf_matter:" .. s.id .. "_powder", {
		description = s.name .. " powder",
		drawtype = "nodebox",
		node_box = LEVELLED,
		paramtype = "light",
		paramtype2 = "leveled",
		leveled = 64,
		tiles = { "tf_matter_substance.png^[multiply:" .. tint },
		groups = groups(false),
		drop = "",
		_matter = { matter = s.id, share = 1, form = true, loose = true },
		_matter_generated = true,
	})
	-- the liquid, where it has one at 1 atm: a full block, and a puddle
	-- levelled by how much is there; the world's matter runs both (sprint 80)
	local liquid = s.boils == nil or s.boils > s.melts
	if liquid and s.melts < 3500 then
		local hot = s.melts > 700
		local def = {
			description = "Liquid " .. s.name:lower() .. " (" .. s.formula .. ")\n1 m³ · " .. weight(s.density),
			drawtype = "liquid",
			tiles = { "tf_matter_liquid.png^[multiply:" .. tint },
			use_texture_alpha = "blend",
			paramtype = "light",
			light_source = hot and 10 or 0,
			walkable = false,
			pointable = true,
			buildable_to = false,
			liquid_move_physics = true,
			drowning = 1,
			damage_per_second = hot and 6 or 0,
			post_effect_color = { a = 160, r = 120, g = 120, b = 130 },
			groups = groups(s.state == "liquid"),
			_matter = { matter = s.id, share = 1, state = "liquid", form = true },
			_matter_generated = true,
		}
		core.register_node("tf_matter:" .. s.id .. "_liquid", def)
		local puddle = table.copy(def)
		puddle.description = "Liquid " .. s.name:lower()
		puddle.drawtype = "nodebox"
		puddle.node_box = LEVELLED
		puddle.paramtype2 = "leveled"
		puddle.leveled = 64
		puddle.groups = groups(false)
		puddle.groups.matter_puddle = 1 -- sprint 81: liquid is a fill now; these become one on load
		puddle.drop = ""
		puddle.liquid_move_physics = false -- waded through (Luanti swims by the block, not by how deep it is)
		puddle.drowning = 0
		puddle.post_effect_color = nil
		core.register_node("tf_matter:" .. s.id .. "_puddle", puddle)
	end
end
