// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "database-planet.h"
#include "exceptions.h"
#include "planet/atlas.h"

MapDatabasePlanet::MapDatabasePlanet(std::unique_ptr<MapDatabase> kept, const planet::Atlas &atlas) :
	m_kept(std::move(kept)), m_atlas(atlas)
{
}

static std::optional<v3s32> keptAt(const planet::Atlas &atlas, const v3s16 &pos)
{
	const std::optional<planet::Atlas::Spot> s = atlas.blockSpot({ pos.X, pos.Y, pos.Z });
	return s ? std::optional(v3s32(s->x, s->y, s->z)) : std::nullopt;
}

bool MapDatabasePlanet::saveBlock(const v3s16 &pos, std::string_view data)
{
	const std::optional<v3s32> at = keptAt(m_atlas, pos);
	return at ? m_kept->saveBlockAt(*at, data) : true; // seam: made again as it was
}

void MapDatabasePlanet::loadBlock(const v3s16 &pos, std::string *block)
{
	block->clear();
	if (const std::optional<v3s32> at = keptAt(m_atlas, pos))
		m_kept->loadBlockAt(*at, block);
}

bool MapDatabasePlanet::deleteBlock(const v3s16 &pos)
{
	const std::optional<v3s32> at = keptAt(m_atlas, pos);
	return at ? m_kept->deleteBlockAt(*at) : true;
}

void MapDatabasePlanet::listAllLoadableBlocks(std::vector<v3s16> &)
{
	throw BaseException("a planet's blocks aren't all in the map at once: listing them is not wired yet");
}
