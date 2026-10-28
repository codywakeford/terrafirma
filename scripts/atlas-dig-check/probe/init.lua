-- Digging on an atlas world (scripts/atlas-dig-check.sh, sprint 30): the
-- ground round the equator emerged; then nodes dug one at a time down a
-- shaft, the server's steps after each timed (it relights round every
-- change, and sends what changed). Prints
-- ATLAS lines; the script reads them.

local atlas = assert(core.planet.atlas(), "atlas_dig: not an atlas world")
local BUDGET = 0.05 -- s late: a dig the player doesn't feel

local top = assert(core.planet.atlas_node(0, math.rad(3), 4), "atlas_dig: no node there")
local min = { x = top.x - 24, y = top.y - 48, z = top.z - 24 }
local max = { x = top.x + 24, y = top.y + 16, z = top.z + 24 }

-- the most a server step ran late since the last dig: past the dedicated
-- server's tick (dtime is never shorter than that)
local TICK = assert(tonumber(core.settings:get("dedicated_server_step")), "atlas_dig: no dedicated_server_step")
local longest = 0
core.register_globalstep(function(dtime) longest = math.max(longest, dtime - TICK) end)

core.after(0, function()
	core.emerge_area(min, max, function(_, _, remaining)
		if remaining > 0 then return end
		local pass, fail, worst = 0, 0, 0
		-- a shaft down from the surface, a layer every half second (each dig's
		-- relighting and resending in the steps after it)
		local layer = 4
		local function dig()
			if layer < -36 then
				print(string.format("ATLAS worst %.3f s late", worst))
				print(string.format("ATLAS pass %d fail %d", pass, fail))
				core.request_shutdown()
				return
			end
			local pos = core.planet.atlas_node(0, math.rad(3), layer)
			if pos and core.planet.atlas_cell(pos) then
				longest = 0
				core.dig_node(pos)
				local at = layer
				core.after(0.5, function()
					worst = math.max(worst, longest)
					if longest > BUDGET then
						fail = fail + 1
						print(string.format("ATLAS slow after digging layer %d (%d,%d,%d): a step %.3f s late", at, pos.x, pos.y, pos.z, longest))
					else
						pass = pass + 1
					end
					layer = layer - 1
					dig()
				end)
			else
				layer = layer - 1
				dig()
			end
		end
		dig()
	end)
end)
