// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "planet/atlas.h"
#include "planet/planet.h"
#include <optional>

#include "mapgen.h"
#include "terragen/world.h"
#include "mapgen/terra_column.h"
#include <string>

class Biome;
class BiomeManager;

// The game's blocks for everything the terra generator places (and the far
// view draws). Throws if the game lacks one.
terra::Blocks terraBlocks(const BiomeManager *bmgr, const NodeDefManager *ndef);

struct MapgenTerraParams : public MapgenParams
{
	// The world code (TF5.…): everything terragen needs. Written into the
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
	// Fills stone, water and air from terragen; records each column. Returns
	// the highest stone placed.
	s16 generateTerrain();
	// Fills the biome map (the engine's passes read it). The soil is painted
	// as the terrain is filled: terra::Columns::node.
	void paintSurface();
	// The finished column at (x, z), or false beyond the polar edge.
	bool groundAt(s16 x, s16 z, terragen::Ground &ground) const;

	terragen::World m_world;
	std::optional<planet::Planet> m_planet; // the world's, if it's one (planet_half)
	std::optional<planet::Atlas> m_atlas;   // or the world's atlas (planet_atlas)
	content_t c_seam = CONTENT_IGNORE;      // an atlas's storage that is no cell
	std::optional<terra::Column> m_atlasColumn; // the last atlas node's column, and its place
	std::array<double, 2> m_atlasPlace{};
	MapNode atlasNode(s16 x, s16 y, s16 z);
	const terra::Column &atlasColumn(const std::array<double, 2> &place);
	// The debug sphere's block for a cell: what it is, painted.
	content_t atlasPaint(const planet::Atlas::Cell &cell) const;
	// An atlas's light, from the planet, not the map: sunlit above its
	// column's ground (storage columns are no columns of the planet).
	void lightAtlas();
	bool m_atlasDebug = false;
	struct AtlasPaints
	{
		content_t segmentA, segmentB, cap, core, segmentSeam, capEdge, cut, coreEdge, line;
	} m_paint{};
	terra::Columns m_columns; // what goes in each column (shared with the far view)
	Biome *m_biomes[(int)terragen::Biome::Count];

	// per column of the current chunk (z outer, x inner)
	std::vector<terra::Column> m_column;
};
