-- Exercise the map request against the actual craft Lua, with a predictable
-- flight and surface. The live flight check covers the native physics.
local modpath = assert(arg[1], "tf_flight mod path required")
local entity, on_ui
local surface_calls = 0
core = {
	get_modpath = function() return modpath end,
	register_entity = function(_, def) entity = def end,
	register_on_ui_event = function(fn) on_ui = fn end,
	planet = {
		state = function(pos, vel) return { pos = { x = pos.x, y = pos.y - 1, z = pos.z }, vel = vel } end,
		fly = function(pos, vel, _, _, dt)
			return { pos = { x = pos.x + dt, y = pos.y - vel.y * dt, z = pos.z }, vel = vel }
		end,
	},
	orbit = {
		elements = function() return { period = 180 } end,
		where = function(state)
			return { longitude = state.pos.x / 100, latitude = state.pos.z / 100, altitude = state.pos.y }
		end,
	},
	terra = { surface = function()
		surface_calls = surface_calls + 1
		return { ground = 5 }
	end },
}
vector = { round = function(v) return v end }
dofile(modpath .. "/craft.lua")({ body = {}, pack = { craft = { mass = 100, drag_area = 0.7 } }, sea = 1, boundary = 100 })
assert(entity and on_ui, "craft must register its UI event")

local published
local craft = setmetatable({ name = "tf_flight:craft", resting = false,
	state = { pos = { x = 0, y = 20, z = 0 }, vel = { x = 0, y = 40, z = 0 } } }, { __index = entity })
local player = {
	get_attach = function() return { get_luaentity = function() return craft end } end,
	set_ui = function(_, key, value)
		assert(key == "map_trajectory")
		published = value
	end,
}
on_ui(player, "map.trajectory", {})
assert(published.ending == "ground" and #published.points >= 2)
assert(surface_calls > 0 and published.seconds > 0 and published.seconds < 0.5)
assert(math.abs(published.points[#published.points][3] - 4.5) < 1e-6, "end at terrain height above sea")

surface_calls = 0
craft.state = { pos = { x = 0, y = 10000, z = 0 }, vel = { x = 10, y = 0, z = 0 } }
on_ui(player, "map.trajectory", {})
assert(published.ending == "time" and #published.points == 193)
assert(surface_calls == 0, "orbital samples don't query terrain")

craft.resting = true
on_ui(player, "map.trajectory", {})
assert(published == nil, "resting craft clears the path")
print("flight trajectory: ok")
