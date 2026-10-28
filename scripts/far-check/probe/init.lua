-- The far view draws each column as core.terra.surface says the generator
-- makes it. Generate some areas for real and compare, column by column:
-- the ground (its block and height) and any water over it. Prints FAR
-- lines; scripts/far-check.sh reads them.

-- one area in the middle of each of six regions: segments round the
-- equator, and a cap (in storage a region's grid is columns of the planet:
-- x east, y the layer, z north)
local SIZE = 96
local AREAS = {}
local atlas = core.planet.atlas() or error("far check: the world isn't a planet")
for _, r in ipairs(atlas.regions) do
	if #AREAS < 6 and r.width > SIZE and r.depth > SIZE and (r.kind == "cap" or r.index % 5 == 0) then
		AREAS[#AREAS + 1] = { r.x + math.floor((r.width - SIZE) / 2), r.z + math.floor((r.depth - SIZE) / 2) }
	end
end
assert(#AREAS == 6, "far check: only " .. #AREAS .. " regions to probe")

local function kind(name)
	local def = core.registered_nodes[name]
	if name == "air" or not def then return "air" end
	local g = def.groups or {}
	if (g.tree or 0) > 0 or (g.leaves or 0) > 0 then return "tree" end
	if def.drawtype == "plantlike" or def.drawtype == "nodebox" then return "plant" end -- planted after: tufts, cacti
	if def.liquidtype ~= "none" or name == "tf_world:ice" then return "liquid" end -- a frozen sea's top is its water
	return "solid"
end

-- the real column, top down
local function real(data, area, x, z, top, bottom)
	local out = {}
	for y = top, bottom, -1 do
		local name = core.get_name_from_content_id(data[area:index(x, y, z)])
		local k = kind(name)
		if k == "tree" and not out.tree then out.tree = { y = y, name = name } end
		if k == "liquid" and not out.water then out.water = { y = y, name = name } end
		if k == "solid" then out.ground = { y = y, name = name } return out end
	end
	return out
end

local totals = { columns = 0, ground = 0, water = 0, rooted = 0 }
local shown = 0
local function miss(what, x, z, want, got)
	if shown < 12 then
		print(("FAR miss %s at %d %d: far %s, world %s"):format(what, x, z, want, got))
		shown = shown + 1
	end
end

local pending = #AREAS
core.after(0, function()
	for _, a in ipairs(AREAS) do
		local minp = { x = a[1], y = -80, z = a[2] }
		local maxp = { x = a[1] + SIZE - 1, y = 255, z = a[2] + SIZE - 1 }
		core.emerge_area(minp, maxp, function(_, _, remaining)
			if remaining > 0 then return end
			local vm = core.get_voxel_manip(minp, maxp)
			local e1, e2 = vm:read_from_map(minp, maxp)
			local area, data = VoxelArea(e1, e2), vm:get_data()
			-- the edges' trees lean in from chunks outside the area: skip a margin
			for z = minp.z + 12, maxp.z - 12 do
				for x = minp.x + 12, maxp.x - 12 do
					local s = core.terra.surface({ x = x, y = 0, z = z })
					local r = real(data, area, x, z, maxp.y, minp.y)
					-- a tree's trunk stands in its column's top block: on a planet
					-- surface plants no trees, so that column can't be told
					local rooted = kind(core.get_name_from_content_id(data[area:index(x, s.ground, z)])) == "tree"
					if rooted then
						totals.rooted = totals.rooted + 1
					else
					totals.columns = totals.columns + 1
					if r.ground and r.ground.y == s.ground and r.ground.name == s.top then
						totals.ground = totals.ground + 1
					else
						local around = {}
						for y = s.ground + 2, s.ground - 1, -1 do
							around[#around + 1] = y .. "=" .. core.get_name_from_content_id(data[area:index(x, y, z)])
						end
						miss("ground", x, z, s.ground .. " " .. s.top, (r.ground and (r.ground.y .. " " .. r.ground.name) or "none")
							.. " [" .. table.concat(around, " ") .. "]")
					end
					local sw = s.water and (s.water .. " " .. s.liquid) or "none"
					local rw = r.water and (r.water.y .. " " .. r.water.name) or "none"
					-- water that has flowed since it was generated is the world's doing, not the generator's
					if r.water and r.water.name:find("_flowing$") then
						totals.water = totals.water + 1
					elseif sw == rw then totals.water = totals.water + 1 else miss("water", x, z, sw, rw) end
					end
				end
			end
			pending = pending - 1
			if pending == 0 then
				print(("FAR columns %d ground %d water %d rooted %d"):format(totals.columns, totals.ground, totals.water, totals.rooted))
				core.request_shutdown()
			end
		end)
	end
end)
