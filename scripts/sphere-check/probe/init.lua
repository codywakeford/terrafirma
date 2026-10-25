-- The sphere check's eye: looking straight down, no clouds. On a planet,
-- over face sphere_probe_face at fraction (sphere_probe_fx, _fz) of it, and
-- each face's ground painted its own block so the joins show; on a flat
-- world, over the map's origin.
local s = core.settings
local Y = tonumber(s:get("sphere_probe_y") or "500")
local planet = core.planet.enabled()

-- sphere_probe_walk: the client walks itself (continuous_forward) north off
-- face 1, where the grid turns a quarter onto face 4; the server reports
-- where it ended up and which way it's looking.
if planet and s:get_bool("sphere_probe_walk") then
	core.register_on_joinplayer(function(player)
		core.after(4, function()
			local sq = core.planet.square(1)
			player:set_pos({ x = sq.x + sq.width / 2, y = 9.5, z = sq.z + sq.width - 6 })
			player:set_look_horizontal(0) -- north
			player:set_look_vertical(0)
			core.after(8, function()
				local p, look = player:get_pos(), player:get_look_dir()
				print(("SPHERE walk face %s look %.2f %.2f y %.1f"):format(
					tostring(core.planet.face(vector.round(p))), look.x, look.z, p.y))
			end)
		end)
	end)
	return
end

core.register_on_joinplayer(function(player)
	core.after(4, function()
		local pos = { x = 0, y = Y, z = 0 }
		if planet then
			local sq = core.planet.square(tonumber(s:get("sphere_probe_face") or "0"))
			pos.x = sq.x + sq.width * tonumber(s:get("sphere_probe_fx") or "0.5")
			pos.z = sq.z + sq.width * tonumber(s:get("sphere_probe_fz") or "0.5")
		end
		player:set_physics_override({ gravity = 0 })
		player:set_clouds({ density = 0 })
		player:set_pos(pos)
		player:set_look_horizontal(0)
		player:set_look_vertical(1.5) -- straight down
	end)
end)

if not planet then
	return
end

local PAINT = { "tf_world:snow", "tf_world:sand", "tf_world:grass", "tf_world:stone_bricks", "tf_world:clay", "tf_world:gravel" }
local ids = {}
for i, n in ipairs(PAINT) do ids[i - 1] = core.get_content_id(n) end
local AIR = core.get_content_id("air")
core.register_on_generated(function(minp, maxp)
	if minp.y > 64 or maxp.y < -32 then
		return -- the flat ground is at y 8
	end
	local vm, emin, emax = core.get_mapgen_object("voxelmanip")
	local area = VoxelArea(emin, emax)
	local data = vm:get_data()
	for z = minp.z, maxp.z do
		for x = minp.x, maxp.x do
			local f = core.planet.face({ x = x, y = 0, z = z })
			if f then
				for y = maxp.y, minp.y, -1 do
					local i = area:index(x, y, z)
					if data[i] ~= AIR then data[i] = ids[f] break end
				end
			end
		end
	end
	vm:set_data(data)
	vm:write_to_map()
end)
