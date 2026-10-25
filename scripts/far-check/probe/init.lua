-- The far view draws each column as core.terra.surface says the generator
-- makes it. Generate some areas for real and compare, column by column:
-- the ground (its block and height), any water over it, and the top of any
-- tree. Prints FAR lines; scripts/far-check.sh reads them.

-- one area on each of the planet's faces: three in the middle, three by an edge
local AREAS = {}
for face = 0, 5 do
	local sq = core.planet.square(face)
	local at = face < 3 and 0.5 or 0.03
	AREAS[#AREAS + 1] = { sq.x + math.floor(sq.width * at), sq.z + math.floor(sq.width * 0.4) }
end
local SIZE = 96

local function kind(name)
	local def = core.registered_nodes[name]
	if name == "air" or not def then return "air" end
	local g = def.groups or {}
	if (g.tree or 0) > 0 or (g.leaves or 0) > 0 then return "tree" end
	if def.drawtype == "plantlike" or def.drawtype == "nodebox" then return "plant" end -- planted after: tufts, cacti
	if def.liquidtype ~= "none" then return "liquid" end
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

local totals = { columns = 0, ground = 0, water = 0, tree = 0, trees = 0, height = 0 }
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
					totals.columns = totals.columns + 1
					if r.ground and r.ground.y == s.ground and r.ground.name == s.top then
						totals.ground = totals.ground + 1
					else
						miss("ground", x, z, s.ground .. " " .. s.top, r.ground and (r.ground.y .. " " .. r.ground.name) or "none")
					end
					local sw = s.water and (s.water .. " " .. s.liquid) or "none"
					local rw = r.water and (r.water.y .. " " .. r.water.name) or "none"
					if sw == rw then totals.water = totals.water + 1 else miss("water", x, z, sw, rw) end
					local st = s.canopy and (s.canopy.high .. " " .. s.canopy.name) or "none"
					local rt = r.tree and (r.tree.y .. " " .. r.tree.name) or "none"
					if st == rt then totals.tree = totals.tree + 1 else miss("tree", x, z, st, rt) end
					if r.tree then totals.trees = totals.trees + 1 end
					-- the crown's height alone: where crowns overlap, which tree's blocks win depends on planting order
					if (s.canopy and s.canopy.high) == (r.tree and r.tree.y) then totals.height = totals.height + 1 end
				end
			end
			pending = pending - 1
			if pending == 0 then
				print(("FAR columns %d ground %d water %d tree %d height %d trees %d"):format(
					totals.columns, totals.ground, totals.water, totals.tree, totals.height, totals.trees))
				core.request_shutdown()
			end
		end)
	end
end)
