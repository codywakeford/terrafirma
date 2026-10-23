-- The Lab. Each mod declares its scenarios as data in its own lab.json (read
-- by the main menu too, before any game Lua runs) and gives them behaviour
-- here with tf_lab.behaviour. A scenario's world — built by the menu with
-- tf_lab = <id> in its map settings — builds that one scenario.

tf_lab = {}

local path = core.get_modpath("tf_lab")
dofile(path .. "/registry.lua")
dofile(path .. "/area.lua")
dofile(path .. "/nodes.lua")

-- Scenarios are declared by mods that load after this one, so the stage is
-- set up once they all have (after the registry's own check).
local id = core.get_mapgen_setting("tf_lab")
if id and id ~= "" then
	core.register_on_mods_loaded(function()
		local stage = dofile(path .. "/stage.lua")(id)
		dofile(path .. "/floor.lua")(stage)
		if core.get_mapgen_setting("tf_lab_check") == "true" then
			dofile(path .. "/check.lua")(stage)
		else
			dofile(path .. "/panel.lua")(stage)
		end
	end)
end
