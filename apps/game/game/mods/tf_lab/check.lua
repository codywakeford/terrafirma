-- Headless check (bun check:lab, one world per scenario): builds the
-- scenario, keeps it active without a player, waits out its expect, logs
-- one LAB line, then stops the server.

return function(stage)
	local s = stage.scenario

	local function forceload()
		local b = stage.bounds
		for x = b.min.x, b.max.x + 15, 16 do
			for _, y in ipairs({ -1, 0 }) do
				for z = b.min.z, b.max.z + 15, 16 do
					if not core.forceload_block({ x = x, y = y, z = z }, true, -1) then
						error("tf_lab check: could not keep " .. s.id .. " loaded")
					end
				end
			end
		end
	end

	do
		core.after(1, function()
			stage.build(function()
				local expect = s.behaviour.expect
				if not expect then
					core.log("action", "LAB " .. s.id .. " visual")
					core.request_shutdown("lab check done")
					return
				end
				forceload()
				core.after(expect.after, function()
					local ok, why = expect.check(stage.area, stage.values)
					core.log("action", "LAB " .. s.id .. " " .. (ok and "ok" or ("fail " .. tostring(why))))
					core.request_shutdown("lab check done")
				end)
			end)
		end)
	end
end
