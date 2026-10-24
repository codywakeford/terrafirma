-- The sphere check's eye: high over a tiny planet's face, looking straight down, no clouds.
core.register_on_joinplayer(function(player)
	core.after(3, function()
		player:set_physics_override({ gravity = 0 })
		player:set_clouds({ density = 0 })
		player:set_pos({ x = 0, y = tonumber(core.settings:get("sphere_probe_y") or "500"), z = 0 })
		player:set_look_horizontal(0)
		player:set_look_vertical(1.5) -- straight down
	end)
end)
