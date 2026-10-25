-- The Lab page's server side for this scenario: what the page shows
-- (set_ui "lab"), what it asks for, and the camera.
--
-- Two modes per player. Browsing (the page's Lab view is open, which is how
-- a scenario starts): the camera orbits the plot (set_ui "camera"). Stepped
-- in (the view is closed): the camera is the player's again, standing by the
-- plot. The page says which as its view opens and closes.

local SPEEDS = { 0, 1, 10, 100 } -- × normal time; 0 pauses
local NORMAL = 72                -- the engine's default time_speed
local ORBIT_PERIOD = 45          -- seconds per turn around the plot

return function(stage)
	local s = stage.scenario
	local speed = 1
	local browsing = {} -- player name → bool

	local function send(player)
		player:set_ui("lab", {
			id = s.id,
			values = stage.values,
			browsing = browsing[player:get_player_name()],
			speed = speed,
			speeds = SPEEDS,
		})
	end

	local function send_all()
		for _, player in ipairs(core.get_connected_players()) do send(player) end
	end

	local function frame(player)
		local on = browsing[player:get_player_name()]
		player:set_ui("camera", on and { orbit = stage.orbit(ORBIT_PERIOD) } or nil)
	end

	local function place(player)
		local view = stage.view()
		player:set_pos(view.pos)
		player:set_look_horizontal(view.yaw)
		player:set_look_vertical(view.pitch)
	end

	-- What a control's behaviour gets to work with.
	local ctx = {
		area = stage.area,
		values = stage.values,
		rebuild = stage.rebuild,
		players = core.get_connected_players,
	}

	local EVENTS = {
		["lab.control"] = function(player, payload)
			local ok, control, value = tf_lab.accept(s, payload.id, payload.value)
			if not ok then return control end
			if control.kind ~= "button" then stage.values[control.id] = value end
			if s.behaviour.control then
				s.behaviour.control(control.id, value, ctx)
			else
				stage.rebuild()
			end
			send_all()
		end,
		["lab.restart"] = function()
			stage.rebuild()
		end,
		["lab.speed"] = function(player, payload)
			for _, v in ipairs(SPEEDS) do
				if payload.speed == v then
					speed = v
					core.set_time_speed(NORMAL * v)
					send_all()
					return
				end
			end
			return "no speed " .. tostring(payload.speed)
		end,
		["lab.browse"] = function(player)
			browsing[player:get_player_name()] = true
			frame(player)
			send(player)
		end,
		["lab.step_in"] = function(player)
			browsing[player:get_player_name()] = false
			place(player)
			frame(player)
			send(player)
		end,
	}

	core.register_on_joinplayer(function(player)
		local function arrive()
			browsing[player:get_player_name()] = true
			place(player)
			frame(player)
			if s.behaviour.enter then s.behaviour.enter(player, stage.values) end
			send(player)
		end
		if stage.built then arrive() else stage.build(arrive) end
	end)

	core.register_on_leaveplayer(function(player)
		browsing[player:get_player_name()] = nil
	end)

	core.register_on_ui_event(function(player, name, payload)
		local handle = EVENTS[name]
		if not handle then return end -- another system's event
		if not stage.built then
			core.log("warning", "tf_lab: " .. name .. " before the scenario was built")
			return
		end
		local refused = handle(player, type(payload) == "table" and payload or {})
		if refused then
			core.log("warning", "tf_lab: " .. name .. " from " .. player:get_player_name() .. ": " .. refused)
		end
	end)

	core.after(0, function()
		core.set_timeofday(0.3) -- start in the morning, once the world runs
	end)
end
