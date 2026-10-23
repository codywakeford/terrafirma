// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "mapgen.h"
#include "terragen/world.h"
#include <string>

class Biome;

struct MapgenTerraParams : public MapgenParams
{
	// The world code (TF3.…): everything terragen needs. Written into the
	// world's map_meta.txt when the world is created.
	std::string code;

	void readParams(const Settings *settings);
	void writeParams(Settings *settings) const;
};

/*
	Terra Firma's generator: terrain, climate and biome come from terragen, a
	pure function of the world code (src/terragen). The engine's own passes —
	ores, decorations, dust, liquids, lighting — run on top, driven by the
	biome terragen chose for each column.
*/
class MapgenTerra : public MapgenBasic
{
public:
	MapgenTerra(MapgenTerraParams *params, EmergeParams *emerge);
	~MapgenTerra() = default;

	virtual MapgenType getType() const { return MAPGEN_TERRA; }

	virtual void makeChunk(BlockMakeData *data);
	int getSpawnLevelAtPoint(v2s16 p);
	int getGroundLevelAtPoint(v2s16 p);

private:
	// Fills stone, water and air from terragen; records each column's surface
	// and biome. Returns the highest stone placed.
	s16 generateTerrain();
	// Paints each column with its biome's surface nodes and fills the biome map.
	void paintSurface();
	// The finished column at (x, z), or false beyond the polar edge.
	bool groundAt(s16 x, s16 z, terragen::Ground &ground) const;

	terragen::World m_world;
	Biome *m_biomes[(int)terragen::Biome::Count];
	content_t c_ice;
	// the game's blocks for terragen's rocks and ores (aliases mapgen_rock_*,
	// mapgen_ore_*), gold-bearing river gravel (mapgen_placer), lava
	content_t c_rock[(int)terragen::Rock::Count];
	content_t c_ore[(int)terragen::Ore::Count];
	content_t c_placer;

	// per column of the current chunk (z outer, x inner)
	std::vector<s32> m_surface;
	std::vector<s32> m_water;             // water surface, or below m_surface when dry
	std::vector<terragen::Water> m_kind;
	std::vector<terragen::Biome> m_column_biome;
	std::vector<bool> m_polar;
	std::vector<bool> m_placer;           // its river gravels carry gold
};
