-- Vectors as { x, y, z } tables: pure values, no state.
return {
	add = function(a, b) return { x = a.x + b.x, y = a.y + b.y, z = a.z + b.z } end,
	sub = function(a, b) return { x = a.x - b.x, y = a.y - b.y, z = a.z - b.z } end,
	scale = function(v, k) return { x = v.x * k, y = v.y * k, z = v.z * k } end,
	dot = function(a, b) return a.x * b.x + a.y * b.y + a.z * b.z end,
	length = function(v) return math.sqrt(v.x * v.x + v.y * v.y + v.z * v.z) end,
	zero = function() return { x = 0, y = 0, z = 0 } end,
	-- a local vector (x east, y up, z north) into a frame's turning-frame
	-- vector (core.orbit.frame), and back
	out = function(f, l)
		return {
			x = f.east.x * l.x + f.up.x * l.y + f.north.x * l.z,
			y = f.east.y * l.x + f.up.y * l.y + f.north.y * l.z,
			z = f.east.z * l.x + f.up.z * l.y + f.north.z * l.z,
		}
	end,
	into = function(f, v)
		return {
			x = f.east.x * v.x + f.east.y * v.y + f.east.z * v.z,
			y = f.up.x * v.x + f.up.y * v.y + f.up.z * v.z,
			z = f.north.x * v.x + f.north.y * v.y + f.north.z * v.z,
		}
	end,
}
