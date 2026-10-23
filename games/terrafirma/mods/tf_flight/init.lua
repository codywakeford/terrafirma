-- Flight. A craft carries its rider under the planet's gravity and through
-- its air (core.orbit: the physics is the engine's, the same as the wiki's
-- cannonball). Two regimes, one craft:
--
-- - near: over the blocks, where a block is a metre. The craft flies the
--   world's own space and lands on whatever it meets.
-- - orbit: above the flight ceiling the blocks let go. The craft flies the
--   planet itself (the world is a map of it: core.terra.globe), the page
--   draws the globe and the orbit, and coming back down through the ceiling
--   puts it over the block under where it now is.
--
-- The flight pack (creative) is the first craft: unlimited propellant, thrust
-- as you aim it.

-- Worlds terra didn't generate (the Lab) have no planet to fly.
if not core.terra.enabled() then
	return
end

local CEILING = 1500    -- m above the sea where the blocks let go
local SEND = 0.2        -- s between readings for the page
local PATH = 128        -- points of the orbit drawn
local WARP = 20         -- how much faster time runs in orbit, Ctrl held, not burning
local PACK = {
	craft = { mass = 100, drag_area = 0.7 }, -- a person, roughly
	thrust = 1.6 * 9.81 * 100,               -- N: enough to climb
	boost = 3,                               -- Ctrl held, near the ground
}

local planet = core.orbit.planet()
local code = core.terra.world().code
local sea = tonumber(core.get_mapgen_setting("water_level")) or error("tf_flight: no water_level")

local function scale(v, k) return { x = v.x * k, y = v.y * k, z = v.z * k } end
local function add(a, b) return { x = a.x + b.x, y = a.y + b.y, z = a.z + b.z } end
local function length(v) return math.sqrt(v.x * v.x + v.y * v.y + v.z * v.z) end

-- ---- the regimes ----------------------------------------------------------

-- Near: the state is the craft's block position with y from the sea, metres.
local function nearState(pos, vel)
	return { pos = { x = pos.x, y = pos.y - sea, z = pos.z }, vel = vel }
end

-- Up through the ceiling: from the block it's over, to the planet.
local function toOrbit(near)
	local g = core.terra.globe(vector.round({ x = near.pos.x, y = 0, z = near.pos.z }))
	return core.orbit.at({ latitude = g.latitude, longitude = g.longitude, altitude = near.pos.y, velocity = near.vel })
end

-- Down through the ceiling: to the block under where it is on the planet.
local function toNear(state)
	local w = core.orbit.where(state)
	local b = core.terra.block(w.latitude, w.longitude)
	return { pos = { x = b.x, y = w.altitude, z = b.z }, vel = w.velocity }
end

-- The thrust the rider asks for, in newtons, from what they hold.
local function thrustNear(player, state)
	local c = player:get_player_control()
	local t = PACK.thrust * (c.aux1 and PACK.boost or 1)
	local push = { x = 0, y = 0, z = 0 }
	if c.up then push = add(push, player:get_look_dir()) end
	if c.jump then push = add(push, { x = 0, y = 1, z = 0 }) end
	if c.down and length(state.vel) > 0.5 then push = add(push, scale(state.vel, -1 / length(state.vel))) end
	local n = length(push)
	return n > 0 and scale(push, t / n) or push
end

-- In orbit: W along the motion, S against it, Space away from the planet,
-- Shift toward it, A/D across the orbit's plane.
local function thrustOrbit(player, state)
	local c = player:get_player_control()
	local d = core.orbit.directions(state)
	local push = { x = 0, y = 0, z = 0 }
	if c.up then push = add(push, d.prograde) end
	if c.down then push = add(push, scale(d.prograde, -1)) end
	if c.jump then push = add(push, d.radial) end
	if c.sneak then push = add(push, scale(d.radial, -1)) end
	if c.left then push = add(push, d.normal) end
	if c.right then push = add(push, scale(d.normal, -1)) end
	local n = length(push)
	return n > 0 and scale(push, PACK.thrust / n) or push, c
end

-- ---- the craft --------------------------------------------------------------

-- Where a move from `from` to `to` (block positions) first meets something
-- solid or liquid: the loaded blocks, and where they aren't loaded yet, the
-- ground as the generator will make it.
local function meets(from, to)
	for hit in core.raycast(from, to, false, true) do
		if hit.type == "node" then
			local def = core.registered_nodes[core.get_node(hit.under).name]
			if def and (def.walkable or def.liquidtype ~= "none") then
				return { x = to.x, y = hit.under.y + 0.5, z = to.z }
			end
		end
	end
	local s = core.terra.surface(vector.round(to))
	local top = math.max(s.ground, s.water or s.ground)
	if to.y < top + 0.5 and core.get_node(vector.round(to)).name == "ignore" then
		return { x = to.x, y = top + 0.5, z = to.z }
	end
end

local Craft = {
	initial_properties = {
		physical = false,
		pointable = false,
		visual = "sprite",
		textures = { "blank.png" },
		static_save = false, -- a rider's flight is kept with the rider (see below)
	},
}

function Craft:mount(player, record)
	self.rider = player:get_player_name()
	self.regime = record and record.regime or "near"
	self.state = record and record.state or nearState(self.object:get_pos(), player:get_velocity())
	self.resting = record == nil and player:get_velocity().y == 0
	self.sent = 0
	player:set_attach(self.object, "", { x = 0, y = 0, z = 0 }, { x = 0, y = 0, z = 0 })
	-- the rider's own walking doesn't fight the craft
	player:set_physics_override({ speed = 0, jump = 0, gravity = 0 })
end

function Craft:dismount(player)
	player:set_detach()
	player:set_physics_override({ speed = 1, jump = 1, gravity = 1 })
	player:set_ui("flight", nil)
	player:get_meta():set_string("tf_flight", "")
	self.object:remove()
end

function Craft:record()
	return { regime = self.regime, state = self.state }
end

function Craft:stepNear(player, dtime)
	local thrust = thrustNear(player, self.state)
	local burning = length(thrust) > 0
	if self.resting and not burning then
		if player:get_player_control().sneak then self:dismount(player) end
		return
	end
	self.resting = false
	local from = self.object:get_pos()
	local next = core.orbit.near(self.state, PACK.craft, thrust, dtime)
	local to = { x = next.pos.x, y = next.pos.y + sea, z = next.pos.z }
	local landed = meets(from, to)
	if landed then
		self.state = nearState(landed, { x = 0, y = 0, z = 0 })
		self.resting = true
		self.object:set_pos(landed)
		self.object:set_velocity({ x = 0, y = 0, z = 0 })
		return
	end
	self.state = next
	if next.pos.y > CEILING and next.vel.y > 0 then
		self.regime = "orbit"
		self.state = toOrbit(next)
		self.object:set_velocity({ x = 0, y = 0, z = 0 }) -- it waits at the ceiling while the craft flies the planet
		return
	end
	self.object:set_pos(to)
	self.object:set_velocity(next.vel)
end

function Craft:stepOrbit(player, dtime)
	local thrust, c = thrustOrbit(player, self.state)
	local warp = (c.aux1 and length(thrust) == 0) and WARP or 1
	self.state = core.orbit.step(self.state, PACK.craft, thrust, dtime * warp)
	local w = core.orbit.where(self.state)
	if w.altitude < CEILING and w.velocity.y < 0 then
		self.regime = "near"
		self.state = toNear(self.state)
		local pos = { x = self.state.pos.x, y = self.state.pos.y + sea, z = self.state.pos.z }
		self.object:set_pos(pos)
		self.object:set_velocity(self.state.vel)
		-- the ground under it, ready before it gets there
		core.emerge_area(vector.offset(vector.round(pos), -32, -CEILING - 64, -32), vector.offset(vector.round(pos), 32, 0, 32))
	end
end

function Craft:report(player)
	local e
	if self.regime == "orbit" then
		e = core.orbit.elements(self.state)
	else
		e = { altitude = self.state.pos.y, speed = length(self.state.vel) }
	end
	local reading = {
		regime = self.regime,
		code = code,
		ceiling = CEILING,
		radius = planet.radius,
		altitude = e.altitude,
		speed = e.speed,
		climb = self.state.vel.y,
		resting = self.resting or nil,
	}
	if self.regime == "orbit" then
		local w = core.orbit.where(self.state)
		reading.climb = w.velocity.y
		reading.latitude, reading.longitude = w.latitude, w.longitude
		reading.orbital_speed = e.orbital_speed
		reading.apoapsis, reading.periapsis, reading.period = e.apoapsis, e.periapsis, e.period
		reading.inclination = e.inclination
		reading.density = core.orbit.density(e.altitude)
		local path = {}
		for i, p in ipairs(core.orbit.path(self.state, PATH)) do
			path[i] = { p.latitude, p.longitude, p.altitude }
		end
		reading.path = path
	end
	player:set_ui("flight", reading)
end

function Craft:on_step(dtime)
	local player = self.rider and core.get_player_by_name(self.rider)
	if not player or player:get_attach() ~= self.object then
		self.object:remove()
		return
	end
	if self.regime == "near" then self:stepNear(player, dtime) else self:stepOrbit(player, dtime) end
	if not self.object:is_valid() then return end -- dismounted
	self.sent = self.sent + dtime
	if self.sent >= SEND then
		self.sent = 0
		self:report(player)
		player:get_meta():set_string("tf_flight", core.serialize(self:record()))
	end
end

core.register_entity("tf_flight:craft", Craft)

local function launch(player, record)
	local obj = core.add_entity(player:get_pos(), "tf_flight:craft")
	if not obj then error("tf_flight: the craft couldn't be added") end
	obj:get_luaentity():mount(player, record)
end

core.register_tool("tf_flight:pack", {
	description = "Flight Pack\nRight-click to strap in. W thrusts where you look, Space straight up, S brakes, Ctrl for more.\n"
		.. "Above the ceiling: W/S along and against your orbit, Space/Shift up and down, A/D across it, Ctrl to wait faster.\n"
		.. "Shift on the ground to unstrap.",
	inventory_image = "tf_flight_pack.png",
	groups = { creative_functional = 1 },
	on_secondary_use = function(stack, user)
		if user:get_attach() then return end
		launch(user, nil)
	end,
	on_place = function(stack, user)
		if user:get_attach() then return end
		launch(user, nil)
	end,
})

-- A rider who leaves mid-flight is still flying when they come back.
core.register_on_joinplayer(function(player)
	local saved = player:get_meta():get_string("tf_flight")
	if saved ~= "" then
		local record = core.deserialize(saved) or error("tf_flight: a saved flight can't be read")
		core.after(0, function()
			if player:is_valid() then launch(player, record) end
		end)
	end
end)
