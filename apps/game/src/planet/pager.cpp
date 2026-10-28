// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "pager.h"
#include <limits>
#include <stdexcept>
#include <string>

namespace planet {

Pager::Pager(Atlas &atlas, Hooks hooks) :
	m_atlas(atlas), m_hooks(std::move(hooks)),
	m_needed((size_t)Atlas::SLOTS * Atlas::SLOTS, -std::numeric_limits<double>::infinity())
{
}

bool Pager::all()
{
	const std::vector<Atlas::TileId> tiles = m_atlas.allTiles();
	if (tiles.size() > m_needed.size())
		return false;
	for (size_t s = 0; s < tiles.size(); s++) {
		m_atlas.put((int)s, tiles[s]);
		m_needed[s] = std::numeric_limits<double>::infinity();
		m_hooks.moved((int)s);
	}
	return true;
}

void Pager::keep(const std::vector<terragen::Atlas::Vec> &points, double now)
{
	for (const terragen::Atlas::Vec &p : points)
		for (const Atlas::TileId &t : m_atlas.tilesNear(p, AROUND))
			need(t, now);
}

int Pager::need(const Atlas::TileId &tile, double now)
{
	if (const std::optional<int> slot = m_atlas.slotOf(tile)) {
		m_needed[*slot] = now;
		return *slot;
	}
	// an empty slot; else the one needed longest ago, past HOLD, that empties
	const std::vector<std::optional<Atlas::TileId>> window = m_atlas.window();
	int chosen = -1;
	for (int s = 0; s < (int)window.size() && chosen < 0; s++)
		if (!window[s])
			chosen = s;
	while (chosen < 0) {
		int oldest = -1;
		for (int s = 0; s < (int)window.size(); s++)
			if (m_needed[s] < now - HOLD && (oldest < 0 || m_needed[s] < m_needed[oldest]))
				oldest = s;
		if (oldest < 0)
			throw std::runtime_error("planet::Pager: the window is full: every one of its " +
					std::to_string(window.size()) + " slots holds a tile needed in the last " +
					std::to_string((int)HOLD) + " s");
		if (m_hooks.empty(oldest))
			chosen = oldest;
		else
			m_needed[oldest] = now; // in use: try it again later
	}
	m_atlas.put(chosen, tile);
	m_needed[chosen] = now;
	m_hooks.moved(chosen);
	return chosen;
}

Atlas::Node Pager::node(const Atlas::Cell &cell, double now)
{
	keep({ m_atlas.atlas().centre(cell) }, now);
	need(m_atlas.tileOf(cell), now); // (a cell high above the ground is in its column's tile)
	return m_atlas.node(cell);
}

} // namespace planet
