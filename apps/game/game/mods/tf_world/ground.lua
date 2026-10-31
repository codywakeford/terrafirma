-- Digging and building on loose ground (sprint 85). Loose ground takes its
-- shape from the columns round it, so a hole dug would ramp the sand round
-- it and a block put down would raise it. Instead the ground round a dug
-- or placed node keeps the shape it had, and slides only as its material
-- does (core.matter.ground_changed): sand slumps into a hole, soil and
-- clay hold its walls.
core.register_on_dignode(function(pos, oldnode)
	core.matter.ground_changed(pos, oldnode)
end)

core.register_on_placenode(function(pos, newnode, placer, oldnode)
	core.matter.ground_changed(pos, oldnode)
end)
