-- The craft: what a flight pack's rider sits in. The rider is attached; the
-- craft flies them over the planet with its own physics (core.planet.fly:
-- gravity weakening with height, the air thinning, the planet turning), up
-- off the ground, round the planet in orbit and back down. Over a face's
-- edge it lands on the next face, and the rider's view turns with the
-- ground. Its speed is true metres a second; across the map it moves by
-- core.terra.across (a block is a metre on the ground, columns widening
-- with height).
--
-- Keys, as walking: W/S forward and back where you look, A/D sideways,
-- Space up, Shift down (S brakes on the ground, Shift down is gravity's
-- job there), Ctrl for more. Right-click the pack again to unstrap.

local V = dofile(core.get_modpath("tf_flight") .. "/vec.lua")

local SEND = 0.2 -- s between readings for the page
local FRICTION = 0.5 * 9.81 -- m/s²: sliding on the ground

-- opts: { body, pack = { craft, thrust, boost }, sea, boundary = m above
-- the sea where the air ends: the page shows the orbit above it }
return function(opts)
	local pack, sea = opts.pack, opts.sea

	-- The push the rider's keys ask for, along the grid (x east, y up, z north), N.
	local function keys(player, state, grounded)
		local c = player:get_player_control()
		local look = player:get_look_dir()
		local side = { x = math.cos(player:get_look_horizontal()), y = 0, z = math.sin(player:get_look_horizontal()) }
		local push = V.zero()
		if c.up then push = V.add(push, look) end
		if c.left then push = V.sub(push, side) end
		if c.right then push = V.add(push, side) end
		if c.jump then push = V.add(push, { x = 0, y = 1, z = 0 }) end
		if grounded then
			local speed = V.length(state.vel)
			if c.down and speed > 0.5 then push = V.add(push, V.scale(state.vel, -1 / speed)) end
		else
			if c.down then push = V.sub(push, look) end
			if c.sneak then push = V.add(push, { x = 0, y = -1, z = 0 }) end
		end
		local n = V.length(push)
		return n > 0 and V.scale(push, pack.thrust * (c.aux1 and pack.boost or 1) / n) or push, c
	end

	-- Where a move from `from` to `to` first meets something solid or liquid:
	-- the loaded blocks, and where they aren't loaded yet, the ground as the
	-- generator will make it. Over a face's edge `to` is on the next face, far
	-- off in the map: the move is taken as straight down the new column.
	local function meets(from, to)
		if core.planet.face(vector.round(from)) ~= core.planet.face(vector.round(to)) then
			from = { x = to.x, y = from.y, z = to.z }
		end
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

	-- The craft's own state: pos in map nodes with y from the sea, vel true
	-- m/s along the grid. From the object's (`vel` in blocks/s).
	local function stateOf(pos, vel)
		local a = core.terra.across(pos.y - sea, pos.z)
		return { pos = { x = pos.x, y = pos.y - sea, z = pos.z }, vel = { x = vel.x * a.east, y = vel.y, z = vel.z * a.north } }
	end

	-- A state's velocity across the map: blocks/s.
	local function blocks(state)
		local a = core.terra.across(state.pos.y, state.pos.z)
		return { x = state.vel.x / a.east, y = state.vel.y, z = state.vel.z / a.north }
	end

	-- A step of the planet's flight; over a face's edge the rider's view turns.
	local function fly(state, thrust, dt, rider)
		local r = core.planet.fly({ x = state.pos.x, y = state.pos.y + sea, z = state.pos.z }, state.vel,
			pack.craft, thrust, dt)
		if r.turn ~= 0 then
			rider:set_look_horizontal(rider:get_look_horizontal() + r.turn * math.pi / 2)
		end
		return { pos = { x = r.pos.x, y = r.pos.y - sea, z = r.pos.z }, vel = r.vel }
	end

	local Craft = {
		initial_properties = {
			physical = false,
			pointable = false,
			visual = "sprite",
			textures = { "blank.png" },
			static_save = false, -- a rider's flight is kept with the rider (init.lua)
		},
	}

	function Craft:mount(player)
		self.rider = player:get_player_name()
		self.state = stateOf(self.object:get_pos(), player:get_velocity())
		self.resting = player:get_velocity().y == 0
		self.sent = 0
		player:set_attach(self.object, "", V.zero(), V.zero())
		opts.body.settle(player)
	end

	function Craft:dismount(player)
		player:set_detach()
		player:get_meta():set_string("tf_flight", "")
		self.object:remove()
		opts.body.settle(player)
		player:set_ui("flight", nil)
	end

	function Craft:fly(player, dtime)
		local thrust = keys(player, self.state, self.resting)
		local burning = V.length(thrust) > 0
		if self.resting and not burning then
			if player:get_player_control().sneak then self:dismount(player) end
			return
		end
		self.resting = false
		local from = self.object:get_pos()
		local next = fly(self.state, thrust, dtime, player)
		local to = { x = next.pos.x, y = next.pos.y + sea, z = next.pos.z }
		local landed = meets(from, to)
		if landed then
			-- on the ground: it slides, friction slowing it, and rests once it's stopped
			local vel = { x = next.vel.x, y = 0, z = next.vel.z }
			local speed = V.length(vel)
			local slowed = math.max(0, speed - FRICTION * dtime)
			vel = speed > 0 and V.scale(vel, slowed / speed) or vel
			self.state = { pos = { x = landed.x, y = landed.y - sea, z = landed.z }, vel = vel }
			self.resting = slowed == 0 and not burning
			self.object:set_pos(landed)
			self.object:set_velocity(blocks(self.state))
			return
		end
		self.state = next
		self.object:set_pos(to)
		self.object:set_velocity(blocks(next))
	end

	-- The page's reading: flying, or above the air the orbit it's on.
	function Craft:report(player)
		local s = self.state
		local reading = {
			regime = "near",
			boundary = opts.boundary,
			altitude = s.pos.y,
			speed = V.length(s.vel),
			climb = s.vel.y,
			resting = self.resting or nil,
		}
		if s.pos.y > opts.boundary then
			local e = core.orbit.elements(core.planet.state({ x = s.pos.x, y = s.pos.y + sea, z = s.pos.z }, s.vel))
			reading = {
				regime = "space",
				boundary = opts.boundary,
				radius = core.orbit.planet().radius,
				altitude = s.pos.y,
				speed = V.length(s.vel),
				orbital_speed = e.orbital_speed,
				climb = s.vel.y,
				apoapsis = e.apoapsis,
				periapsis = e.periapsis,
				period = e.period,
				inclination = e.inclination,
				density = core.orbit.density(s.pos.y),
				riding = true,
				lone = true,
				buildable = true,
			}
		end
		player:set_ui("flight", reading)
	end

	function Craft:on_step(dtime)
		local player = self.rider and core.get_player_by_name(self.rider)
		if not player or player:get_attach() ~= self.object then
			self.object:remove()
			return
		end
		self:fly(player, dtime)
		if not self.object:is_valid() then return end -- dismounted
		self.sent = self.sent + dtime
		if self.sent >= SEND then
			self.sent = 0
			self:report(player)
			player:get_meta():set_string("tf_flight", "flying")
		end
	end

	core.register_entity("tf_flight:craft", Craft)

	return {
		-- Strap a player into a new craft where they are.
		launch = function(player)
			local obj = core.add_entity(player:get_pos(), "tf_flight:craft")
			if not obj then error("tf_flight: the craft couldn't be added") end
			obj:get_luaentity():mount(player)
			return obj
		end,
	}
end
