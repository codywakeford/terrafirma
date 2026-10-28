-- The world as a planet (core.planet, src/planet/atlas.h): its segments,
-- caps and core are regions of the map, and every node that is no cell is
-- the seam: a wall nothing passes, never drawn. The generator places it
-- (mapgen_seam), cell by cell. Stepping from one region into the next is
-- the engine's (core.planet's) business; anything that walks or flows
-- across a seam without it meets the seam, never the wrong region's blocks.

local atlas = core.planet.atlas()
if not atlas then
	return
end

core.register_node("tf_planet:seam", {
	description = "Seam (between the planet's regions)",
	drawtype = "airlike",
	paramtype = "light",
	sunlight_propagates = true, -- no shadow along the joins
	walkable = true,
	pointable = false,
	diggable = false,
	buildable_to = false,
	is_ground_content = false,
	drop = "",
	groups = { not_in_creative_inventory = 1 },
	on_blast = function() end,
})

core.register_alias("mapgen_seam", "tf_planet:seam")

-- TEMP(2026-10-27): the debug sphere's paints (planet_atlas_debug, sprint 30):
-- a cell coloured by what it is, its edges lined so the grid shows.
do
	local PAINTS = {
		segment_a = "#8a9a8f", segment_b = "#a7b5aa", cap = "#5f86c4", core = "#7a5aa6",
		segment_seam = "#e0b050", cap_edge = "#f08a3c", cut = "#e8463a", core_edge = "#d36ca0", line = "#3b4a45",
	}
	for name, colour in pairs(PAINTS) do
		core.register_node("tf_planet:debug_" .. name, {
			description = "Atlas debug: " .. name,
			tiles = { "[fill:16x16:" .. colour .. "^[fill:16x1:0,0:#00000050^[fill:1x16:0,0:#00000050" },
			groups = { cracky = 3, not_in_creative_inventory = 1 },
			is_ground_content = false,
		})
	end
end

tf_planet = {}

-- On land on the equator, east from longitude 0. The spawn level is a
-- layer (the ground's height); the node there is the atlas's (the
-- column's own grid, at that layer).
function tf_planet.spawn_position()
	for lon = 0, 359, 2 do
		local column = core.planet.atlas_node(0, math.rad(lon), 0)
		local layer = column and core.get_spawn_level(column.x, column.z)
		if layer then
			local lat, at = core.planet.atlas_place(column)
			local node = core.planet.atlas_node(lat, at, layer)
			if node then return node end
		end
	end
	error("tf_planet: no land on the equator to spawn on")
end
local function spawn(player)
	player:set_pos(tf_planet.spawn_position())
	return true
end
core.register_on_newplayer(spawn)
core.register_on_respawnplayer(spawn)
