// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "far/globe.h"
#include "far/terrain.h"
#include "irr_ptr.h"
#include "planet/atlas.h"
#include <CMeshBuffer.h>
#include <map>
#include <memory>
#include <tuple>
#include <vector>

namespace far {

class Colours;
class Workers;

/*
	What tiles are chosen for: a point of view on the planet. Anything that
	looks at the planet makes one: the player's camera (far::View), the
	Planetarium's. In blocks.
*/
struct Eye
{
	Globe::Vec at;   // the eye's point over the sea, from the planet's centre
	float altitude;  // above the sea: counts toward how far a tile is seen
	float cut;       // nearer than this along the ground, someone else draws (the near world); 0 for none
	float range;     // farther than this along the ground, nothing is drawn
	float split;     // a tile splits while nearer than this many of its spans past the cut
};

/*
	A planet's land from afar: a quadtree of globe tiles (far/globe.h) for
	an Eye, finer near it and coarser with distance, built on worker
	threads nearest first. A tile keeps drawing until what replaces it is
	ready, so refining never opens holes. Each tile's mesh is in true
	positions on the planet round its middle (BS units): whoever draws it
	places the middle.
*/
class Tiles
{
public:
	// `colours` outlives the tiles: the workers read it.
	Tiles(const planet::Atlas &atlas, const Horizon &horizon, const Colours &colours);
	~Tiles();

	const Globe &globe() const { return m_globe; }

	// Once a frame: the tiles to draw for the eye (falling back to what's
	// ready), the rest asked of the workers, and a few finished ones taken
	// onto the GPU. Throws if building one failed.
	void update(const Eye &eye);

	struct Drawn
	{
		TileKey key;
		Globe::Vec middle; // where its mesh is from, on the planet
		const std::vector<irr_ptr<scene::SMeshBuffer>> &parts;
	};
	// What update() chose, ready to draw.
	std::vector<Drawn> drawn() const;

private:
	struct Key
	{
		s32 level = 0, x = 0, z = 0, face = 0;
		Key() = default;
		Key(const TileKey &k) : level(k.level), x(k.x), z(k.z), face(k.face) {}
		Key(s32 level, s32 x, s32 z, s32 face) : level(level), x(x), z(z), face(face) {}
		bool operator<(const Key &o) const
		{
			return std::tie(level, face, x, z) < std::tie(o.level, o.face, o.x, o.z);
		}
		TileKey tile() const { return { level, x, z, face }; }
		Key child(s32 c) const { return { level - 1, x * 2 + (c & 1), z * 2 + (c >> 1), face }; }
	};
	struct Entry
	{
		std::vector<irr_ptr<scene::SMeshBuffer>> parts;
		bool ready = false;
		u32 used = 0; // the frame it was last wanted or drawn
	};

	// how far a tile is from the eye along the ground (blocks, less half its size)
	float distance(const TileKey &key, const Eye &eye) const;
	bool choose(const Key &key, const Eye &eye, std::vector<Key> &draw, std::vector<std::pair<float, Key>> &want);
	void install();
	void evict();

	const planet::Atlas m_atlas; // kept: the workers build from them
	const Horizon m_horizon;
	const Globe m_globe;
	std::unique_ptr<Workers> m_workers;
	std::map<Key, Entry> m_tiles;
	std::vector<Key> m_draw;
	u32 m_frame = 0;
};

} // namespace far
