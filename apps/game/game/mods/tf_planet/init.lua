-- The world as a planet (core.planet, src/planet/planet.h): six faces laid
-- out in the map as a net. Between them, and off the net, is the seam: a
-- wall nothing passes, never drawn. Stepping from one face onto the next is
-- the engine's (core.planet's) business; anything that walks or flows
-- across a join without it meets the seam, never the wrong face's blocks.

if not core.planet.enabled() then
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

-- Everyone starts near the middle of face 0, on land: the nearest ground the
-- generator would spawn on, searched outward (the engine's own spawn search
-- looks round the map's origin, which is off the net).
local SEARCH = 64 -- blocks between the points tried
local function spawn(player)
	local s = core.planet.square(0)
	local cx, cz = s.x + s.width / 2, s.z + s.width / 2
	for ring = 0, math.floor(s.width / 2 / SEARCH) - 1 do
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
	error("tf_planet: no land on face 0 to spawn on")
end
core.register_on_newplayer(spawn)
core.register_on_respawnplayer(spawn)
