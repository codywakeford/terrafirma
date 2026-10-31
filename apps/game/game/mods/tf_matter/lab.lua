-- tf_matter's Lab scenarios (data in lab.json): chemistry in crucibles
-- (sprint 79). Nothing here knows what reacts: the matter table does.

tf_lab.load()

-- what goes in each crucible: substance → moles
local POTS = {
	{ water = 111, sodium = 0.5 },                         -- 2 L of water, 11 g of sodium
	{ water = 111, hydrogen_chloride = 3, aluminium = 0.5 }, -- acid, 13 g of aluminium
	{ water = 111, sodium_hydroxide = 1, hydrogen_chloride = 1 },
}

local function fill(area)
	for i, pot in ipairs(POTS) do
		local at = area:pos({ x = i * 2 - 1, y = 1, z = 1 })
		for id, moles in pairs(pot) do
			core.matter.add(at, id, moles)
		end
	end
end

-- moles of a substance in a block
local function holds(pos, id)
	local r = core.matter.inspect(pos)
	for _, c in ipairs(r and r.contents or {}) do
		if c.substance == id then return c.moles end
	end
	return 0
end

tf_lab.behaviour("chemistry.bench", {
	build = function(area)
		area:fill({ x = 0, y = 0, z = 0 }, { x = 6, y = 0, z = 2 }, "tf_world:stone")
		for i = 1, #POTS do
			area:set({ x = i * 2 - 1, y = 1, z = 1 }, "tf_matter:crucible")
		end
		-- poured once the crucibles are in the world (the matter's woken there)
		core.after(0, fill, area)
	end,
	-- fresh crucibles, filled again
	control = function(id, value, ctx) ctx.rebuild() end,
	expect = {
		after = 10,
		check = function(area)
			local lye = holds(area:pos({ x = 1, y = 1, z = 1 }), "sodium_hydroxide")
			local dissolved = holds(area:pos({ x = 3, y = 1, z = 1 }), "aluminium_chloride")
			local salt = holds(area:pos({ x = 5, y = 1, z = 1 }), "salt")
			return lye > 0.4 and dissolved > 0.4 and salt > 0.9,
				string.format("caustic soda %.2f mol, aluminium chloride %.2f mol, salt %.2f mol", lye, dissolved, salt)
		end,
	},
})
