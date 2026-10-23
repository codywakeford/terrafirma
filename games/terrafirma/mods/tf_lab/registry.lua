-- Scenarios: data from each mod's lab.json, behaviour from its Lua. Both are
-- checked here, and once every mod has loaded each must have the other, so a
-- mistake stops the game loading with the reason.
--
-- lab.json:
--   { "scenarios": [ {
--       "id": "group.name", "title": "…", "description": "what to watch for",
--       "size": { "x": 7, "y": 3, "z": 7 },
--       "world": { "mapgen": "singlenode" },              (optional)
--       "camera": { "radius": 14, "height": 6 },          (optional framing)
--       "controls": [                                      (optional)
--         { "id": "gap", "kind": "slider", "label": "Gap", "min": 1, "max": 5, "step": 1, "default": 3, "unit": "blocks" },
--         { "id": "on", "kind": "toggle", "label": "Vent", "default": false },
--         { "id": "stuff", "kind": "choice", "label": "Material", "options": [ { "value": "sand", "label": "Sand" } ], "default": "sand" },
--         { "id": "drop", "kind": "button", "label": "Drop" } ] } ] }
--
-- tf_lab.behaviour(id, {
--   build = function(area, values) end,             -- place its blocks; values: control id → value
--   control = function(id, value, ctx) end,         -- optional; without it, any change rebuilds
--   enter = function(player, values) end,           -- optional: a player arrived
--   expect = { after = 5, check = function(area, values) return ok, why end }, -- optional, headless
-- })

tf_lab.scenarios = {} -- id → scenario (the JSON, plus .group and .behaviour)

local KINDS = {
	button = function(c) return true end,
	toggle = function(c) return type(c.default) == "boolean" end,
	slider = function(c)
		return type(c.min) == "number" and type(c.max) == "number" and type(c.step) == "number"
			and c.min < c.max and c.step > 0 and type(c.default) == "number"
			and c.default >= c.min and c.default <= c.max
	end,
	choice = function(c)
		if type(c.options) ~= "table" or #c.options == 0 then return false end
		for _, o in ipairs(c.options) do
			if type(o.label) ~= "string" or o.value == nil then return false end
			if o.value == c.default then return true end
		end
		return false
	end,
}

local function fail(where, why)
	error("tf_lab: " .. where .. ": " .. why, 0)
end

local function check(s, where)
	if type(s.id) ~= "string" or not s.id:match("^[a-z_]+%.[a-z0-9_]+$") then fail(where, "id must look like group.name") end
	where = where .. " " .. s.id
	if tf_lab.scenarios[s.id] then fail(where, "declared twice") end
	if type(s.title) ~= "string" or type(s.description) ~= "string" then fail(where, "needs a title and a description") end
	local z = s.size
	if type(z) ~= "table" or not (z.x and z.y and z.z) or z.x < 1 or z.y < 1 or z.z < 1 or z.x > 48 or z.z > 48 then
		fail(where, "size must be { x, y, z } blocks, at most 48 across")
	end
	if s.camera ~= nil and (type(s.camera) ~= "table" or (s.camera.radius or 1) <= 0) then
		fail(where, "camera must be { radius?, height? }, radius positive")
	end
	s.controls = s.controls or {}
	local ids = {}
	for _, c in ipairs(s.controls) do
		if type(c.id) ~= "string" or ids[c.id] then fail(where, "every control needs its own id") end
		ids[c.id] = true
		local ok = KINDS[c.kind]
		if not ok then fail(where, "control " .. c.id .. ": no kind " .. tostring(c.kind)) end
		if type(c.label) ~= "string" or not ok(c) then fail(where, "control " .. c.id .. " is malformed for a " .. c.kind) end
	end
end

-- Reads the calling mod's lab.json.
function tf_lab.load()
	local mod = core.get_current_modname()
	local path = core.get_modpath(mod) .. "/lab.json"
	local file = io.open(path, "r")
	if not file then fail(mod, "no lab.json") end
	local data = core.parse_json(file:read("*a"))
	file:close()
	if type(data) ~= "table" or type(data.scenarios) ~= "table" then fail(path, "needs a \"scenarios\" array") end
	for _, s in ipairs(data.scenarios) do
		check(s, mod)
		s.group = s.id:match("^([a-z_]+)%.")
		tf_lab.scenarios[s.id] = s
	end
end

function tf_lab.behaviour(id, b)
	local s = tf_lab.scenarios[id]
	if not s then fail(id, "no such scenario in this mod's lab.json (call tf_lab.load() first)") end
	if s.behaviour then fail(id, "behaviour given twice") end
	if type(b.build) ~= "function" then fail(id, "needs build(area, values)") end
	for _, hook in ipairs({ "control", "enter" }) do
		if b[hook] ~= nil and type(b[hook]) ~= "function" then fail(id, hook .. " must be a function") end
	end
	if b.expect ~= nil and (type(b.expect.after) ~= "number" or type(b.expect.check) ~= "function") then
		fail(id, "expect must be { after = seconds, check = function }")
	end
	s.behaviour = b
end

-- Each control's starting value.
function tf_lab.defaults(s)
	local values = {}
	for _, c in ipairs(s.controls) do
		if c.kind ~= "button" then values[c.id] = c.default end
	end
	return values
end

-- What each kind of control accepts from the page: ok, value — or false, why.
local ACCEPT = {
	button = function() return true, nil end,
	toggle = function(c, v)
		if type(v) ~= "boolean" then return false, "not true/false" end
		return true, v
	end,
	slider = function(c, v)
		if type(v) ~= "number" or v < c.min or v > c.max then return false, "out of range" end
		return true, v
	end,
	choice = function(c, v)
		for _, o in ipairs(c.options) do
			if o.value == v then return true, v end
		end
		return false, "not an option"
	end,
}

-- A value from the page for one of a scenario's controls: ok, control,
-- value — or false, why.
function tf_lab.accept(s, id, value)
	for _, c in ipairs(s.controls) do
		if c.id == id then
			local ok, v = ACCEPT[c.kind](c, value)
			if not ok then return false, c.id .. ": " .. v end
			return true, c, v
		end
	end
	return false, "no control " .. tostring(id)
end

core.register_on_mods_loaded(function()
	for id, s in pairs(tf_lab.scenarios) do
		if not s.behaviour then fail(id, "declared in lab.json but no mod gives it a behaviour") end
	end
end)
