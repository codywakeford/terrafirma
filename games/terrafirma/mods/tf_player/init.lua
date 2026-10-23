-- What the player may do and use: how they move (below), and whether they
-- have the wiki (at the end).
--
-- How the player moves. Luanti gives singleplayer every privilege except
-- fly, fast and noclip: games grant those, and ours grants them in creative
-- worlds (double-tap jump to fly, as Minecraft). Sprinting is ours in every
-- mode, as Minecraft's: hold Ctrl (aux1), or double-tap forward; it lasts
-- until you stop moving forward. A run, not Luanti's fast mode.

local SPRINT = 1.6 -- × walking speed
local CREATIVE = { fly = true, fast = true, noclip = true }
local creative = core.settings:get_bool("creative_mode", false)

-- Match the player inventory's nine-column layout. Existing worlds may have
-- items in the old 3×3 craft list, so move them before resizing it.
core.register_on_joinplayer(function(player)
	local inv = player:get_inventory()
	local craft = inv:get_list("craft") or {}
	for i, stack in ipairs(craft) do
		if not stack:is_empty() then
			local leftover = inv:add_item("main", stack)
			if not leftover:is_empty() then
				core.add_item(player:get_pos(), leftover)
			end
			inv:set_stack("craft", i, "")
		end
	end
	inv:set_size("craft", 4)
	inv:set_width("craft", 2)
	inv:set_size("main", math.max(inv:get_size("main"), 36))
	inv:set_width("main", 9)
	player:hud_set_hotbar_itemcount(9)
	if creative then
		player:set_ui("creative", true)
	end
end)

core.register_on_joinplayer(function(player)
	if not creative then
		return
	end
	local name = player:get_player_name()
	local privs = core.get_player_privs(name)
	for priv in pairs(CREATIVE) do
		privs[priv] = true
	end
	core.set_player_privs(name, privs)
end)

-- The catalogue is only a view. A server event checks the world's mode and
-- the registered definition before replacing the chosen hotbar slot.
core.register_on_ui_event(function(player, name, payload)
	if name ~= "creative.take" and name ~= "creative.clear" then return end
	if not creative or type(payload) ~= "table" then return end
	local slot = payload.slot
	local inv = player:get_inventory()
	if type(slot) ~= "number" or slot % 1 ~= 0 or slot < 0 or slot >= inv:get_size("main") then return end
	if name == "creative.clear" then
		inv:set_stack("main", slot + 1, "")
		return
	end
	local item, count = payload.item, payload.count
	if type(item) ~= "string" or type(count) ~= "number" then return end
	if count % 1 ~= 0 or count < 1 then return end
	local def = core.registered_items[item]
	if not def or item == "" or item:match("_flowing$") or
			def.description == "" or (def.groups and def.groups.not_in_creative_inventory) then return end
	local stack = ItemStack(item)
	stack:set_count(math.min(count, stack:get_stack_max()))
	inv:set_stack("main", slot + 1, stack)
end)

local DOUBLE_TAP = 0.3 -- s between two presses of forward that start a sprint

local sprinting = {} -- player name → whether the override is on
local forward = {}   -- player name → { down = was forward held, tapped = when it was last pressed, latched = sprinting from a double tap }

core.register_globalstep(function()
	local now = core.get_us_time() / 1e6
	for _, player in ipairs(core.get_connected_players()) do
		local name = player:get_player_name()
		local control = player:get_player_control()
		local f = forward[name] or { down = false, tapped = -1, latched = false }
		if control.up and not f.down then -- forward pressed now
			f.latched = now - f.tapped < DOUBLE_TAP
			f.tapped = now
		end
		if not control.up then f.latched = false end
		f.down = control.up
		forward[name] = f
		local on = control.up and (control.aux1 or f.latched)
		if on ~= (sprinting[name] or false) then
			player:set_physics_override({ speed = on and SPRINT or 1 })
			sprinting[name] = on
		end
	end
end)

core.register_on_leaveplayer(function(player)
	sprinting[player:get_player_name()] = nil
	forward[player:get_player_name()] = nil
end)

-- The wiki (key G): offered where the world's rules allow it (tf_wiki in
-- world.mt, chosen on the create screen's Rules). Worlds made before the
-- rule existed have no tf_wiki and keep it, as Classic does.
local rules = Settings(core.get_worldpath() .. "/world.mt")
local wiki = rules:get_bool("tf_wiki", true)

core.register_on_joinplayer(function(player)
	if wiki then
		player:set_ui("wiki", true)
	end
end)
