-- Pouring (sprint 80). Hold the pour key (X) with something in hand that's
-- made of matter: a block of a substance, a scoop, the world's water, sand.
-- It runs out in a stream at a rate that builds while it's held, into the
-- face you're looking at (or a crucible, or the liquid you're pointing
-- into), as a liquid or as a powder that heaps. A tap pours one cup (a
-- sixteenth of a block each way: 1/4096 m³, about a quarter of a litre).
--
-- What lands is the world's matter's: liquids run and level, powders heap
-- and slide. Nothing here moves it. A block item is a cubic metre: poured,
-- it empties a cup at a time (its description says how much is left) and
-- is gone when it's all out. In creative, it never runs out.

local CUP = 1 / 4096    -- m³
local CUPS = 4096       -- in a block
local START = 4         -- cups a second when the key goes down
local FASTEST = 256     -- cups a second, after holding it a few seconds (a sixteenth of a block)
local REACH = 6         -- blocks

local substances = {}
core.register_on_mods_loaded(function()
	for _, s in ipairs(core.matter.substances()) do substances[s.id] = s end
end)

-- what a stack pours: a substance, and whether it's a scoop's (which never runs out)
local function contents(stack)
	if stack:get_name() == "tf_matter:scoop" then
		local id = stack:get_meta():get_string("substance")
		return id ~= "" and id or nil, true
	end
	local def = core.registered_nodes[stack:get_name()]
	local m = def and def._matter
	if type(m) == "string" then m = { matter = m } end
	-- a substance or a block of one thing (water, sand), not air, a vessel or a family's mix
	if type(m) == "table" and m.matter and m.matter ~= "air" and substances[m.matter] then
		return m.matter, false
	end
end

-- where a pour lands: into a crucible or a liquid or heap pointed into,
-- else the face looked at
local function target(user)
	local eye = vector.offset(user:get_pos(), 0, user:get_properties().eye_height, 0)
	local far = vector.add(eye, vector.multiply(user:get_look_dir(), REACH))
	for thing in core.raycast(eye, far, false, true) do
		if thing.type == "node" then
			local def = core.registered_nodes[core.get_node(thing.under).name]
			local m = def and def._matter
			local into = type(m) == "table" and (m.vessel or m.loose or m.state == "liquid")
			-- and the point looked at, for the stream to fall on (sprint 82: not the block's middle)
			return into and thing.under or thing.above, eye, thing.intersection_point
		end
	end
end

-- the stream, drawn from the hand to where it lands: a liquid's as drops,
-- a cup each, that the clients' liquid layers splash into the water there
local function stream(user, eye, pos, colour, rate, cups, liquid)
	local hand = vector.add(eye, vector.multiply(user:get_look_dir(), 0.6))
	if liquid then
		core.matter.stream(vector.offset(hand, 0, -0.2, 0), pos, colour, cups)
		return
	end
	core.add_particlespawner({
		amount = math.min(30, 4 + math.floor(rate / 8)), time = 0.1,
		minpos = vector.offset(hand, -0.04, -0.2, -0.04), maxpos = vector.offset(hand, 0.04, -0.15, 0.04),
		minvel = vector.multiply(vector.subtract(pos, hand), 2.5), maxvel = vector.multiply(vector.subtract(pos, hand), 3),
		minacc = { x = 0, y = -9, z = 0 }, maxacc = { x = 0, y = -9, z = 0 },
		minexptime = 0.25, maxexptime = 0.4, minsize = 0.6, maxsize = 1.4, collisiondetection = true,
		collision_removal = true,
		texture = "tf_matter_spark.png^[multiply:" .. colour,
	})
end

-- pour `cups` from the user's hand; false once there's nothing to pour
local function pour(user, cups, rate)
	local stack = user:get_wielded_item()
	local id, endless = contents(stack)
	if not id then return false end
	local pos, eye, hit = target(user)
	if not pos then return true end
	local s = substances[id]
	local creative = core.is_creative_enabled(user:get_player_name())
	if not endless and not creative then
		-- a block item empties a cup at a time; the last cup takes the item
		local meta = stack:get_meta()
		local out = meta:get_int("poured") + cups
		if out >= CUPS then
			cups = cups - (out - CUPS)
			stack:take_item(1)
			stack:get_meta():set_int("poured", 0)
			stack:get_meta():set_string("description", "")
		else
			meta:set_int("poured", out)
			local def = core.registered_items[stack:get_name()]
			meta:set_string("description", (def.description or stack:get_name()):match("^[^\n]*")
				.. string.format("\n%d%% left", math.floor(100 * (1 - out / CUPS))))
		end
		user:set_wielded_item(stack)
	end
	-- a cup of it as it is at room temperature: kg/m³ to moles, at the temperature the world has there
	local moles = cups * CUP * s.density / (s.molar_mass / 1000)
	if s.state == "gas" then
		core.matter.add(pos, id, moles)
	else
		core.matter.pour(pos, id, moles)
	end
	stream(user, eye, hit or vector.offset(pos, 0, -0.4, 0), s.colour, rate, cups, s.state == "liquid")
	return true
end

local held = {} -- player → { since, owed }
core.register_globalstep(function(dtime)
	for _, user in ipairs(core.get_connected_players()) do
		local name = user:get_player_name()
		if user:get_player_control().pour then
			local h = held[name]
			if not h then
				-- a tap pours a cup at once
				h = { since = 0, owed = 0 }
				held[name] = h
				if not pour(user, 1, START) then held[name] = { since = 0, owed = -math.huge } end
			else
				h.since = h.since + dtime
				local rate = math.min(FASTEST, START * 2 ^ (h.since * 2)) -- doubling twice a second
				h.owed = h.owed + rate * dtime
				if h.owed >= 1 then
					local cups = math.floor(h.owed)
					h.owed = h.owed - cups
					if not pour(user, cups, rate) then h.owed = -math.huge end
				end
			end
		else
			held[name] = nil
		end
	end
end)

-- A placed block of liquid is that much liquid where it was put, all at
-- once (sprint 82: poured over two seconds, as in sprint 80 to spare a
-- jolt, it vanished and came back as a trickle); it runs out from there.
core.register_on_placenode(function(pos, node)
	local def = core.registered_nodes[node.name]
	local m = def and def._matter
	if type(m) ~= "table" or m.state ~= "liquid" or not substances[m.matter] then return end
	local s = substances[m.matter]
	core.swap_node(pos, { name = "air" })
	-- a liquid at room temperature as it is; a molten one (tin, glass) just over its melting point
	local hot = s.state ~= "liquid" and s.melts + 50 or nil
	core.matter.pour(pos, m.matter, s.density / (s.molar_mass / 1000), hot)
end)
