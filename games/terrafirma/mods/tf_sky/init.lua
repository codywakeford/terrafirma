-- Each player's sky follows the real sun for where they stand and the day of
-- the year (core.terra.sun). The engine places the sun, moon and stars and
-- lights the world from it; this only keeps it told. Worlds terra didn't
-- generate (the Lab) have no real sun to follow and keep the stock sky; the
-- Lab shows the solar sky through its own scenarios (lab.lua).

dofile(core.get_modpath("tf_sky") .. "/lab.lua") -- its Lab scenario

if not core.terra.enabled() then
	return
end

local PERIOD = 1.0  -- seconds between checks
local CHANGE = 0.02 -- degrees of latitude or declination worth sending

local sent = {} -- player name → the solar sky they have

local function update(player)
	local name = player:get_player_name()
	local sun = core.terra.sun(player:get_pos())
	local last = sent[name]
	if last and math.abs(last.latitude - sun.latitude) < CHANGE
			and math.abs(last.declination - sun.declination) < CHANGE then
		return
	end
	local solar = { latitude = sun.latitude, declination = sun.declination }
	player:set_sky({ solar = solar })
	sent[name] = solar
end

core.register_on_joinplayer(update)

core.register_on_leaveplayer(function(player)
	sent[player:get_player_name()] = nil
end)

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
