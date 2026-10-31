-- Objects (sprint 49): things made off the world's grid, a cart or a pot.
-- The engine makes, holds, edits and moves them (src/workshop; physics,
-- sprint 57); this entity only carries one (its `piece` property: the
-- machine's text, saved as its staticdata). Physical, so players stand on
-- it and bump into it.

core.register_entity("tf_world:piece", {
	initial_properties = {
		visual = "piece",
		physical = true,
		collide_with_objects = true,
		pointable = true,
		static_save = true,
	},
	on_activate = function(self, staticdata)
		if staticdata == "" then
			error("tf_world:piece: made without its object")
		end
		self.object:set_properties({ piece = staticdata })
		self.object:set_armor_groups({ immortal = 1 })
	end,
	get_staticdata = function(self)
		return self.object:get_properties().piece
	end,
})

-- What drives a machine (sprint 69): fittings (sprint 73), blocks that do
-- something besides weigh. Held in build mode, Add fits one whole to the
-- workpiece, facing the way you look (its facedir). A seat on a machine's
-- body is where its driver sits, and its front is ahead (right-click the
-- machine to sit, Shift to get up); each engine turns the wheels on driven
-- joints with its power. Each weighs what it says (`mass`, kg), not its
-- block of matter: they're made things (sprint 78). `_machine` is read by tf_matter when it weighs
-- blocks.
local seat = "tf_goodvibes_oak_log.png^[colorize:#6b3a1f:110"
core.register_node("tf_world:seat", {
	description = "Seat (fitted to a machine: its driver sits here, facing its front)",
	-- top, bottom, sides, then its back (+z: ahead, for facedir 0) and its
	-- backrest (-z), darker
	tiles = { "tf_goodvibes_oak_log_top.png^[colorize:#6b3a1f:110", seat, seat, seat, seat,
		"tf_goodvibes_oak_log.png^[colorize:#3a1f10:170" },
	paramtype2 = "facedir",
	groups = { choppy = 2 },
	_matter = { matter = "wood", share = 0.5 },
	_machine = { role = "seat", mass = 25 },
})

core.register_node("tf_world:motor", {
	description = "Engine (fitted to a machine: turns its driven wheels)",
	tiles = { "tf_goodvibes_stone_bricks.png^[colorize:#3c4048:170" },
	paramtype2 = "facedir",
	groups = { cracky = 2 },
	-- a block of it is mostly iron: an engine's weight
	_matter = { matter = "iron", share = 0.4 },
	-- a strong car engine's (sprint 78): 370 kW (500 hp) to 7000 rpm, its
	-- block as heavy as one
	_machine = { role = "motor", mass = 220, power = 370, rpm = 7000 },
})
