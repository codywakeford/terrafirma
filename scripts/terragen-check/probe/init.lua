-- Emerges the columns listed in the world's points.txt, logs each column's
-- (looking past small plants and cactus, which stand on the ground)
-- water surface and solid surface and what core.terra says there (with the
-- clock it read), then stops the server.
local points = {}
for line in io.lines(core.get_worldpath() .. "/points.txt") do
	local x, z = line:match("(-?%d+) (-?%d+)")
	points[#points + 1] = { x = tonumber(x), z = tonumber(z) }
end
local remaining = #points

local WATER = { ["tf_world:water_source"] = true, ["tf_world:water_flowing"] = true, ["tf_world:ice"] = true,
	["tf_world:lava_source"] = true, ["tf_world:lava_flowing"] = true }

local function column(p)
	local water = -9999
	for y = 260, -120, -1 do
		local name = core.get_node({ x = p.x, y = y, z = p.z }).name
		if WATER[name] then
			if water == -9999 then water = y end
		elseif name ~= "air" and name ~= "ignore" and core.get_item_group(name, "plant") == 0 and name ~= "tf_world:cactus" then
			return y, name, water
		end
	end
	return nil, "none", water
end

core.register_on_mods_loaded(function()
	core.after(1, function()
		for _, p in ipairs(points) do
			core.emerge_area({ x = p.x, y = -160, z = p.z }, { x = p.x, y = 260, z = p.z }, function(_, _, left)
				if left > 0 then return end
				local y, name, water = column(p)
				-- and the block 20 below the surface: rock, ore or cave
				local deep = y and core.get_node({ x = p.x, y = y - 20, z = p.z }).name or "none"
				core.log("action", string.format("PROBE %d %d %s %s %d %s", p.x, p.z, tostring(y), name, water, deep))
				local pos = { x = p.x, y = 0, z = p.z }
				local sun, climate = core.terra.sun(pos), core.terra.climate(pos)
				local calendar = core.terra.calendar()
				local geology = core.terra.geology(pos)
				core.log("action", string.format("TERRA %d %d %d %.17g %d %d %.6f %.6f %.6f %.6f %.6f %s %s %.6f %s",
					p.x, p.z, core.get_day_count(), core.get_timeofday(), calendar.year, calendar.day, calendar.date,
					sun.latitude, sun.declination, sun.elevation, sun.daylight, sun.season,
					climate.biome, climate.temperature, geology.province:gsub(" ", "_")))
				local w = core.terra.weather({ x = p.x, y = math.max(y or 0, 1), z = p.z }) -- at the ground, where the climate is
				core.log("action", string.format("WEATHER %d %d %.4f %.4f %.4f %.4f %.4f %d %d %.4f",
					p.x, p.z, w.temperature, climate.temperature, w.precipitation, w.pressure, w.cloud, #w.events, core.get_day_count(), core.get_timeofday()))
				-- and what that weather does to the top of the column (snow, ice), applied
				local top = water ~= -9999 and { x = p.x, y = water, z = p.z } or (y and { x = p.x, y = y, z = p.z })
				if top then
					local node = core.get_node(top)
					local here = core.terra.weather(top)
					local action = tf_weather.act(top, node) or "none"
					local now = core.get_node(action == "snow" and vector.offset(top, 0, 1, 0) or top).name
					core.log("action", string.format("ACT %d %d %s %s %s %.3f %s %.4f", p.x, p.z, node.name, action, now,
						here.temperature, tostring(here.snow), here.precipitation))
				end
				remaining = remaining - 1
				if remaining == 0 then core.request_shutdown("probe done") end
			end)
		end
	end)
end)
