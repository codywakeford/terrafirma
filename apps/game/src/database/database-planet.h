// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "database.h"
#include <memory>

namespace planet { class Atlas; }

/*
	A planet's map blocks, kept by where they are on the planet: a block of
	a tile in the window is saved at its spot (planet::Atlas::blockSpot),
	so a tile moved to another slot finds its blocks again. A block of no
	tile (the seam between slots) is the seam the generator makes there:
	never kept.
*/
class MapDatabasePlanet : public MapDatabase
{
public:
	MapDatabasePlanet(std::unique_ptr<MapDatabase> kept, const planet::Atlas &atlas);

	bool saveBlock(const v3s16 &pos, std::string_view data) override;
	void loadBlock(const v3s16 &pos, std::string *block) override;
	bool deleteBlock(const v3s16 &pos) override;
	// Every kept block isn't a place in the map (only the window's are):
	// throws.
	void listAllLoadableBlocks(std::vector<v3s16> &dst) override;

	void beginSave() override { m_kept->beginSave(); }
	void endSave() override { m_kept->endSave(); }
	bool initialized() const override { return m_kept->initialized(); }
	void verifyDatabase() override { m_kept->verifyDatabase(); }

private:
	std::unique_ptr<MapDatabase> m_kept;
	const planet::Atlas &m_atlas;
};
