-- Soil as a composition. One block whose param2 is what it's made of: its
-- mineral part on the soil texture triangle (sand, silt, clay) and its
-- organic matter, one of the points terragen defines (core.terra.soil_points)
-- and chooses for each place (src/terragen/soil.cpp). Its colour comes from
-- its palette; its name from the soil texture classes soil scientists use.

local POINTS = core.terra.soil_points()

-- The USDA soil texture classes, by the shares of sand, silt and clay.
local function texture(sand, silt, clay)
	if clay >= 0.4 then
		return silt >= 0.4 and "silty clay" or sand >= 0.45 and "sandy clay" or "clay"
	elseif clay >= 0.27 then
		return sand > 0.45 and "sandy clay loam" or silt >= 0.4 and "silty clay loam" or "clay loam"
	elseif silt >= 0.8 and clay < 0.12 then
		return "silt"
	elseif silt >= 0.5 then
		return "silt loam"
	elseif sand >= 0.85 then
		return "sand"
	elseif sand >= 0.7 then
		return "loamy sand"
	elseif clay >= 0.2 and sand > 0.45 then
		return "sandy clay loam"
	elseif sand >= 0.52 or (clay < 0.07 and silt < 0.5) then
		return "sandy loam"
	end
	return "loam"
end

local function name(k)
	local p = POINTS[k + 1]
	if not p then return "Soil" end
	local class = texture(p.sand, p.silt, p.clay)
	local peat = p.organic >= 0.3 and "Peaty " or ""
	local label = peat .. class
	return string.format("%s%s (sand %d%% · silt %d%% · clay %d%%)", label:sub(1, 1):upper(), label:sub(2),
		math.floor(p.sand * 100 + 0.5), math.floor(p.silt * 100 + 0.5), math.floor(p.clay * 100 + 0.5))
end

-- each point's matter: quartz for the sand and silt, kaolinite for the clay,
-- organic matter as cellulose (it burns, and peat burns best)
local function made_of(p)
	local mineral = 1 - p.organic
	local parts = {}
	local quartz, clay = (p.sand + p.silt) * mineral, p.clay * mineral
	if quartz > 0 then parts.silica = quartz end
	if clay > 0 then parts.kaolinite = clay end
	if p.organic > 0 then parts.wood = p.organic end
	return parts
end

local points = {}
for i, p in ipairs(POINTS) do points[i] = made_of(p) end

core.register_node("tf_world:soil", {
	description = "Soil",
	tiles = { "tf_world_soil.png" },
	paramtype2 = "color",
	palette = "tf_world_soil_palette.png",
	groups = { crumbly = 3, soil = 1 },
	_family = { share = 0.55, points = points }, -- grains with air between them
	_point_name = name,
})
core.register_alias("mapgen_soil", "tf_world:soil")
