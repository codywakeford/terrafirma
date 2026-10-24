-- The craft: what a flight pack's rider sits in. The rider is attached;
-- the craft flies them, in one of two places:
--
-- - near: over the planet's blocks, under its gravity and through its air
--   (core.orbit.near). Its speed is true metres a second; across the map it
--   moves by core.terra.across, so a block is a metre near the ground and
--   the planet it maps up high, as the far view draws it. Up through the
--   boundary it goes into space (space.enter).
-- - space: in a bubble's cell. Alone in its bubble, its thrust moves the
--   bubble's orbit (it carries the rider's whole patch of space); sharing a
--   bubble (a station), it moves about the cell. A bubble falling back
--   through the boundary brings it down (land).
--
-- Keys, as walking: W/S forward and back where you look, A/D sideways,
-- Space up, Shift down (near: S brakes, Shift down is gravity's job), Ctrl
-- for more; alone in space, Ctrl coasting runs time faster. Right-click the
-- pack again to unstrap.

local V = dofile(core.get_modpath("tf_flight") .. "/vec.lua")

local SEND = 0.2 -- s between readings for the page
local WARP = 20  -- time, Ctrl held coasting alone in space
local FRICTION = 0.5 * 9.81 -- m/s²: sliding on the ground
local LEVEL = 340 -- blocks/s: the most a craft crosses the map sideways (the map loads only so fast)

-- opts: { space, body, pack = { craft, thrust, boost }, sea }
return function(opts)
	local space, pack, sea = opts.space, opts.pack, opts.sea

	-- The push the rider's keys ask for, local (x east, y up, z north), N.
	local function keys(player, state, near)
		local c = player:get_player_control()
		local look = player:get_look_dir()
		local side = { x = math.cos(player:get_look_horizontal()), y = 0, z = math.sin(player:get_look_horizontal()) }
		local push = V.zero()
		if c.up then push = V.add(push, look) end
		if c.left then push = V.sub(push, side) end
		if c.right then push = V.add(push, side) end
		if c.jump then push = V.add(push, { x = 0, y = 1, z = 0 }) end
		if near then
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
	-- generator will make it.
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

	-- Over the blocks: the craft's own state, pos in map blocks with y from
	-- the sea, vel true m/s. `vel` here is the object's (blocks/s).
	local function nearState(pos, vel)
		local a = core.terra.across(pos.y - sea, pos.z)
		return { pos = { x = pos.x, y = pos.y - sea, z = pos.z }, vel = { x = vel.x * a.east, y = vel.y, z = vel.z * a.north } }
	end

	-- A state's velocity across the map: blocks/s.
	local function blocks(state)
		local a = core.terra.across(state.pos.y, state.pos.z)
		return { x = state.vel.x / a.east, y = state.vel.y, z = state.vel.z / a.north }
	end

	-- A step over the blocks: across the map by the blocks' span here,
	-- crossing it no faster than LEVEL.
	local function near(state, thrust, dt)
		local a = core.terra.across(state.pos.y, state.pos.z)
		local next = core.orbit.near(state, pack.craft, thrust, dt)
		local level = math.sqrt((next.vel.x / a.east) ^ 2 + (next.vel.z / a.north) ^ 2)
		if level > LEVEL then
			next.vel.x, next.vel.z = next.vel.x * LEVEL / level, next.vel.z * LEVEL / level
		end
		next.pos.x = state.pos.x + (next.pos.x - state.pos.x) / a.east
		next.pos.z = state.pos.z + (next.pos.z - state.pos.z) / a.north
		return next
	end

	local REGIMES = {}

	function REGIMES.near(self, player, dtime)
		local thrust = keys(player, self.state, true)
		local burning = V.length(thrust) > 0
		if self.resting and not burning then
			if player:get_player_control().sneak then self:dismount(player) end
			return
		end
		self.resting = false
		local from = self.object:get_pos()
		local next = near(self.state, thrust, dtime)
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
		if next.pos.y > space.boundary and next.vel.y > 0 then
			self.regime = "space"
			self.state = nil
			space.enter(self.object, space.up(next))
			return
		end
		self.object:set_pos(to)
		self.object:set_velocity(blocks(next))
	end

	function REGIMES.space(self, player)
		local b = space.at(self.object:get_pos())
		if not b then return end -- between bubbles for a moment (being restored): space finds it one
		local thrust, c = keys(player, nil, false)
		if space.lone(b, self.object) then
			space.push(b, thrust, (c.aux1 and V.length(thrust) == 0) and WARP or 1)
			self.object:set_velocity(V.zero())
			self.object:set_acceleration(V.zero())
		else
			self.object:set_acceleration(V.scale(thrust, 1 / pack.craft.mass))
		end
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

	function Craft:mount(player, regime)
		self.rider = player:get_player_name()
		self.regime = regime
		self.state = regime == "near" and nearState(self.object:get_pos(), player:get_velocity()) or nil
		self.resting = regime == "near" and player:get_velocity().y == 0
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

	-- Back over the blocks from space: `n` its near state, `pos` where.
	function Craft:land(n, pos)
		self.regime = "near"
		self.state = n
		self.resting = false
		self.object:set_acceleration(V.zero())
		self.object:set_pos(pos)
		self.object:set_velocity(blocks(n))
	end

	-- Over the blocks, the page's reading (in space, space's reporter sends it).
	function Craft:report(player)
		player:set_ui("flight", {
			regime = "near",
			boundary = space.boundary,
			altitude = self.state.pos.y,
			speed = V.length(self.state.vel),
			climb = self.state.vel.y,
			resting = self.resting or nil,
		})
	end

	function Craft:on_step(dtime)
		local player = self.rider and core.get_player_by_name(self.rider)
		if not player or player:get_attach() ~= self.object then
			self.object:remove()
			return
		end
		REGIMES[self.regime](self, player, dtime)
		if not self.object:is_valid() then return end -- dismounted
		self.sent = self.sent + dtime
		if self.sent >= SEND then
			self.sent = 0
			if self.regime == "near" and self.state then self:report(player) end
			player:get_meta():set_string("tf_flight", self.regime)
		end
	end

	core.register_entity("tf_flight:craft", Craft)

	return {
		near = near,
		blocks = blocks,
		-- Strap a player into a new craft where they are.
		launch = function(player, regime)
			local obj = core.add_entity(player:get_pos(), "tf_flight:craft")
			if not obj then error("tf_flight: the craft couldn't be added") end
			obj:get_luaentity():mount(player, regime)
			return obj
		end,
	}
end
