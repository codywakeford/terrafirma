// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "far/globe.h"
#include "far/mesh.h"
#include "far/terrain.h"
#include "planet/atlas.h"
#include "planet/planet.h"
#include "irrlichttypes_bloated.h"
#include <matrix4.h>
#include <memory>
#include <optional>
#include <vector>

class ClientMap;

namespace far {

// The span of a map-block column's surface: from its lowest ground to the
// top of its highest water or tree, in blocks; from a tile at `level`.
struct Surface
{
	s16 bx, bz;
	s32 low, high;
	u8 level;
};

inline u32 columnKey(s32 bx, s32 bz)
{
	return ((u32)(u16)bx << 16) | (u16)bz;
}

/*
	What the far view needs to know of the planet it draws: where its tiles
	are and how far each is from the eye, how a tile is built and drawn, and
	where the near world's blocks stand in the mask. The view (far/view.h)
	chooses, builds and draws tiles the same way on every planet; each kind
	of planet is a layout.

	A cube planet's (NetLayout) tiles lie on its net, the map, and are drawn
	curved by the sphere lens shader, as its near world is. An atlas's
	(GlobeLayout) tiles lie on a globe (far/globe.h), built on the planet as
	its blocks are (AtlasView), knowing nothing of how the atlas stores them.
*/
class Layout
{
public:
	virtual ~Layout() = default;

	// Building tiles (any thread): the land, and a tile's mesh.
	virtual std::unique_ptr<Generator> generator(const Horizon &horizon) const = 0;
	virtual Mesh mesh(const Tile &tile, const Colours &colours) const = 0;
	// The far view's shader, and the planet's radius (blocks).
	virtual const char *shader() const = 0;
	virtual double radius() const = 0;

	// Each frame: where the eye is (map nodes, as the camera is). Then its
	// height over the sea (blocks), and how high its own place's ground is.
	virtual void look(v3f eye, ClientMap &map) = 0;
	virtual float altitude() const = 0;

	// The coarsest tiles that cover what the view reaches, `range` (blocks)
	// round the eye; finer ones are their quarters.
	virtual std::vector<TileKey> roots(float range) const = 0;
	// How far a tile is from the eye (blocks, less half its size).
	virtual float distance(const TileKey &key) const = 0;
	// How a tile's mesh is drawn (to the scene, `origin`: the scene position
	// of map node (0, 0, 0)), or not at all.
	virtual std::optional<core::matrix4> transform(const TileKey &key, v3f origin) const = 0;

	/*
		The mask: a grid of map-block columns round the eye, a texel each,
		where the near world draws (the shader hides the far view there). Its
		corner (scene x, z) and the column a near block stands in; and whether
		a column is the near world's only once it draws all of its surface
		(the land under it known from the far view's finest tiles).
	*/
	virtual v2f maskCorner(v3f origin) const = 0;
	virtual std::optional<v2s32> maskColumn(v3s16 block, v3f origin) const = 0;
	// The map-block column of a texel, where columns are whole (the near
	// world's only once it draws all of their surface: a cube planet's, whose
	// map is the land's) and their surface known; none where they aren't.
	virtual std::optional<u32> surfaceColumn(s32 u, s32 v) const = 0;
	// The surface of each map-block column a tile covers (any thread), where
	// columns are whole.
	virtual std::vector<Surface> surfaces(const Tile &tile) const = 0;
	// Whether the near world draws every column a tile covers (`near`: by
	// texel), so the tile needn't be drawn.
	virtual bool covered(const TileKey &key, const std::vector<u8> &near) const = 0;
};

// The mask's side, in map-block columns.
constexpr s32 MASK = 128;

// A cube planet's: the far view as it has always been.
std::unique_ptr<Layout> netLayout(const planet::Planet &planet, s16 water_level);
// An atlas's.
std::unique_ptr<Layout> globeLayout(const planet::Atlas &atlas, s16 water_level);

} // namespace far
