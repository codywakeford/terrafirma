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
