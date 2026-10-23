-- A scenario's plot, as its build and checks see it: positions are relative
-- to the plot's corner (0, 0, 0) on the floor, +x east, +y up, +z north.

local Area = {}
Area.__index = Area

function tf_lab.Area(min, size)
	return setmetatable({ min = min, size = size }, Area)
end

function Area:pos(rel)
	return vector.add(self.min, rel)
end

function Area:set(rel, name)
	core.set_node(self:pos(rel), { name = name })
end

-- Every block from a to b (inclusive, relative).
function Area:fill(a, b, name)
	for x = math.min(a.x, b.x), math.max(a.x, b.x) do
		for y = math.min(a.y, b.y), math.max(a.y, b.y) do
			for z = math.min(a.z, b.z), math.max(a.z, b.z) do
				self:set({ x = x, y = y, z = z }, name)
			end
		end
	end
end

function Area:get(rel)
	return core.get_node(self:pos(rel)).name
end

-- The middle of the plot's floor.
function Area:centre()
	return { x = math.floor(self.size.x / 2), y = 0, z = math.floor(self.size.z / 2) }
end
