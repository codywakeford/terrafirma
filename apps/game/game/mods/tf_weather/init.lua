-- The weather each player stands in. The atmosphere moves on its own in the
-- engine (src/fluid: wind, rain, storms over the whole world); this reads it
-- where each player is and shows it: the clouds' cover, colour and drift,
-- rain or snow falling around them, ash downwind of a volcano. The page gets
-- the reading too (set_ui "weather"). Worlds terra didn't generate have no
-- atmosphere and keep the stock sky.

dofile(core.get_modpath("tf_weather") .. "/world.lua") -- snow cover and ice, from the weather

if not core.terra.enabled() then
	return
end

local PERIOD = 2.0 -- seconds between readings; each fall lasts as long
local RADIUS = 12  -- blocks around the player that rain falls in

-- What falls, by kind: texture, how many per second per mm/h, how fast, how big.
local FALLS = {
	rain = { texture = "tf_weather_rain.png", rate = 120, speed = 11, size = 2.5, cap = 600 },
	snow = { texture = "tf_weather_snow.png", rate = 80, speed = 1.5, size = 1.2, cap = 400 },
	ash = { texture = "tf_weather_ash.png", rate = 200, speed = 0.8, size = 0.8, cap = 300 },
}

local function fall(player, kind, amount, wind)
	local f = FALLS[kind]
	local count = math.min(f.cap, math.floor(amount * f.rate * PERIOD))
	if count < 1 then
		return
	end
	local drift = { x = wind.x * 0.3, y = -f.speed, z = wind.z * 0.3 }
	core.add_particlespawner({
		amount = count,
		time = PERIOD,
		attached = player,
		minpos = { x = -RADIUS, y = 8, z = -RADIUS },
		maxpos = { x = RADIUS, y = 14, z = RADIUS },
		minvel = drift,
		maxvel = drift,
		minexptime = 14 / f.speed,
		maxexptime = 14 / f.speed,
		minsize = f.size,
		maxsize = f.size,
		collisiondetection = true,
		collision_removal = true,
		vertical = kind == "rain",
		texture = f.texture,
		playername = player:get_player_name(),
	})
end

local space = core.terra.world().space -- north of it, the map is space (tf_flight's bubbles): no weather

local function update(player)
	local pos = player:get_pos()
	if pos.z >= space then
		player:set_clouds({ density = 0 })
		player:set_ui("weather", nil)
		return
	end
	local w = core.terra.weather(pos)
	-- grey with rain, dark in a storm
	local shade = math.floor(255 - math.min(150, w.precipitation * 40))
	player:set_clouds({
		density = 0.15 + 0.8 * w.cloud,
		color = { r = shade, g = shade, b = shade + 5, a = 229 },
		speed = { x = w.wind.x, z = w.wind.z },
	})
	if w.precipitation > 0.05 then
		fall(player, w.snow and "snow" or "rain", w.precipitation, w.wind)
	end
	if w.ash > 0.1 then
		fall(player, "ash", w.ash, w.wind)
	end
	-- events go only when there are any (an empty Lua table has no JSON shape)
	player:set_ui("weather", {
		temperature = w.temperature, humidity = w.humidity, precipitation = w.precipitation,
		snow = w.snow, wind = w.wind, pressure = w.pressure,
		events = #w.events > 0 and w.events or nil,
	})
end

core.register_on_joinplayer(update)

local elapsed = 0
core.register_globalstep(function(dtime)
	elapsed = elapsed + dtime
	if elapsed < PERIOD then
		return
	end
	elapsed = 0
	for _, player in ipairs(core.get_connected_players()) do
		update(player)
	end
end)
