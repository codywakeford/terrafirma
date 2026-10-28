-- The world as a planet (core.planet, src/planet/planet.h): six faces laid
-- out in the map as a net. Between them, and off the net, is the seam: a
-- wall nothing passes, never drawn. Stepping from one face onto the next is
-- the engine's (core.planet's) business; anything that walks or flows
-- across a join without it meets the seam, never the wrong face's blocks.
--
-- An atlas planet (src/planet/atlas.h) is stored the same way: its bands,
-- caps and core are regions of the map, and every node that is no cell is
-- seam. There the generator places it (mapgen_seam), cell by cell.

local cube = core.planet.enabled()
local atlas = core.planet.atlas()
if not cube and not atlas then
	return
end

core.register_node("tf_planet:seam", {
	description = "Seam (between the planet's faces)",
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
if atlas then
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

if atlas then
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
	return
end

-- Everyone starts on land, the nearest the generator would spawn on, searched
-- outward from `x, z` (the engine's own spawn search looks round the map's
-- origin, which is off the net), at most `reach` blocks each way.
local SEARCH = 64 -- blocks between the points tried
local function spawnNear(player, cx, cz, reach, where)
	for ring = 0, math.floor(reach / SEARCH) - 1 do
		for i = -ring, ring do
			for _, p in ipairs({ { i, -ring }, { i, ring }, { -ring, i }, { ring, i } }) do
				local x, z = cx + p[1] * SEARCH, cz + p[2] * SEARCH
				local y = core.get_spawn_level(x, z)
				if y then
					player:set_pos({ x = x, y = y, z = z })
					return true
				end
			end
		end
	end
	error("tf_planet: no land " .. where .. " to spawn on")
end

local SEAM = core.get_content_id("tf_planet:seam")

-- Whether an area's columns are all on one face: its corners are (faces
-- are squares with gaps between them).
local function onOneFace(minp, maxp)
	local f = core.planet.face(minp)
	return f ~= nil
		and core.planet.face({ x = maxp.x, y = 0, z = minp.z }) == f
		and core.planet.face({ x = minp.x, y = 0, z = maxp.z }) == f
		and core.planet.face(maxp) == f
end

-- Whatever the generator made off the faces becomes seam, whole columns at a time.
core.register_on_generated(function(minp, maxp)
	if onOneFace(minp, maxp) then
		return -- nearly every chunk
	end
	local gaps = {}
	for z = minp.z, maxp.z do
		for x = minp.x, maxp.x do
			if not core.planet.face({ x = x, y = 0, z = z }) then
				gaps[#gaps + 1] = { x = x, z = z }
			end
		end
	end
	if #gaps == 0 then
		return
	end
	local vm, emin, emax = core.get_mapgen_object("voxelmanip")
	local area = VoxelArea(emin, emax)
	local data = vm:get_data()
	for _, g in ipairs(gaps) do
		for y = minp.y, maxp.y do
			data[area:index(g.x, y, g.z)] = SEAM
		end
	end
	vm:set_data(data)
	vm:calc_lighting()
	vm:write_to_map()
end)

-- Cube: near the middle of face 0.
local function spawn(player)
	local s = core.planet.square(0)
	return spawnNear(player, s.x + s.width / 2, s.z + s.width / 2, s.width / 2, "on face 0")
end
core.register_on_newplayer(spawn)
core.register_on_respawnplayer(spawn)
