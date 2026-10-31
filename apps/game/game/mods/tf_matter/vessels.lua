-- Vessels (sprint 79): a crucible holds what's put in it, and its matter
-- does what it would: sodium dropped in water fizzes, aluminium in acid
-- gives off hydrogen, sand and soda ash under a torch make glass. The
-- block is air that keeps what's poured into it (`vessel`), so the world's
-- physics runs it like any other. A scoop carries one substance and pours
-- it a cup at a time (a sixteenth of a block: 1/4096 m³, about ¼ litre).

local CUP = 1 / 4096 -- m³
local POURS = 8      -- cups a second, held

core.register_node("tf_matter:crucible", {
	description = "Crucible\nPour into it with a scoop; heat it with a blowtorch",
	drawtype = "nodebox",
	paramtype = "light",
	tiles = { "tf_matter_substance.png^[multiply:#b0703c" },
	node_box = { type = "fixed", fixed = {
		{ -0.4, -0.5, -0.4, 0.4, -0.4, 0.4 },  -- floor
		{ -0.4, -0.4, -0.4, 0.4, 0.3, -0.32 }, -- walls
		{ -0.4, -0.4, 0.32, 0.4, 0.3, 0.4 },
		{ -0.4, -0.4, -0.32, -0.32, 0.3, 0.32 },
		{ 0.32, -0.4, -0.32, 0.4, 0.3, 0.32 },
	} },
	groups = { cracky = 3 },
	_matter = { matter = "air", vessel = true },
})

-- what a scoop holds: the substance of a block (its form or its liquid)
local function substanceOf(pos)
	local def = core.registered_nodes[core.get_node(pos).name]
	local m = def and def._matter
	if type(m) == "table" and m.matter ~= "air" then return m.matter end
	if type(m) == "string" and m ~= "air" then return m end
end

local substances = {}
core.register_on_mods_loaded(function()
	for _, s in ipairs(core.matter.substances()) do substances[s.id] = s end
end)

local function pour(user, stack, cups)
	local id = stack:get_meta():get_string("substance")
	if id == "" then
		core.chat_send_player(user:get_player_name(), "The scoop's empty: right-click a block of something to fill it.")
		return
	end
	local eye = vector.offset(user:get_pos(), 0, user:get_properties().eye_height, 0)
	for thing in core.raycast(eye, vector.add(eye, vector.multiply(user:get_look_dir(), 5)), false, false) do
		if thing.type == "node" then
			local pos = thing.under
			if core.get_node(pos).name ~= "tf_matter:crucible" then
				core.chat_send_player(user:get_player_name(), "Pour into a crucible.")
				return
			end
			local s = substances[id]
			-- a cup of it as it is at room temperature: kg/m³ to moles
			local moles = cups * CUP * s.density / (s.molar_mass / 1000)
			core.matter.add(pos, id, moles)
			tf_matter.probe(user, pos)
			return
		end
	end
end

core.register_tool("tf_matter:scoop", {
	description = "Scoop\nRight-click a block to fill it with that · left-click a crucible to pour a cup (hold: more)",
	inventory_image = "tf_matter_probe.png^[colorize:#c8a060:120",
	groups = { creative_functional = 1 },
	on_use = function(stack, user) pour(user, stack, 1) end,
	on_place = function(stack, user, pointed)
		if pointed.type ~= "node" then return end
		local id = substanceOf(pointed.under)
		if not id then
			core.chat_send_player(user:get_player_name(), "Nothing to scoop there.")
			return
		end
		local meta = stack:get_meta()
		meta:set_string("substance", id)
		meta:set_string("description", "Scoop of " .. substances[id].name:lower())
		return stack
	end,
})

-- held down, it keeps pouring
local pouring = {}
core.register_globalstep(function(dtime)
	for _, user in ipairs(core.get_connected_players()) do
		local name = user:get_player_name()
		local control = user:get_player_control()
		local stack = user:get_wielded_item()
		if (control.dig or control.LMB) and stack:get_name() == "tf_matter:scoop" then
			pouring[name] = (pouring[name] or -0.4) + dtime -- a tap is on_use's one cup
			if pouring[name] >= 1 / POURS then
				pour(user, stack, math.floor(pouring[name] * POURS))
				pouring[name] = pouring[name] % (1 / POURS)
			end
		else
			pouring[name] = nil
		end
	end
end)
