// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "far/layout.h"
#include "far/terrain.h"
#include "terragen/map.h"
#include "irr_ptr.h"
#include "irrlichttypes_bloated.h"
#include <CMeshBuffer.h>
#include <ISceneNode.h>
#include <SMaterial.h>
#include <map>
#include <unordered_map>
#include <memory>
#include <optional>
#include <tuple>
#include <vector>

class Camera;
class ClientMap;
class IShaderSource;
class ITextureSource;
class NodeDefManager;
namespace video { class ITexture; }

namespace far {

class Colours;

class Workers;
struct DrawState;
class Uniforms;

/*
	The far view: the world beyond the loaded map blocks, out to the horizon,
	as blocks (far/terrain.h). A quadtree of tiles around the camera, finer
	near where the real blocks end and coarser with distance, built on
	worker threads nearest first. A tile keeps drawing until what replaces it
	is ready, so refining never opens holes.

	Where the near world draws a map-block column, the far view doesn't: a
	small mask of the columns the near world has meshes for (and draws) is
	read by the far shader, so the seam is exactly the near world's edge.

	Drawn right after the sky with the camera's depth projection. Its depth
	remains for near blocks and clouds, so a cloud behind distant terrain is
	occluded. The planet's curve is applied as it's drawn, never stored.

	The world is a planet, of one kind or another: its layout (far/layout.h)
	says where tiles are and how they're built and drawn, so the far view and
	the near world are one surface; up through the air the haze gives way to
	the whole planet.
*/
class View : public scene::ISceneNode
{
public:
	View(scene::ISceneManager *smgr, IShaderSource *ssrc, ITextureSource *tsrc,
			const NodeDefManager *ndef, const Horizon &horizon, std::unique_ptr<Layout> layout);
	~View();

	// Once a frame, before drawing. `near_range`: how far the map blocks are
	// drawn, in blocks. `fog_limit`: a server's fog distance, in blocks.
	void update(const Camera &camera, ClientMap &map, float near_range, bool range_all,
			std::optional<float> fog_limit);

	// Whether it draws this frame, and how far off what it draws can be
	// (scene units): the fog follows it.
	bool showing() const { return m_showing; }
	float reach() const { return m_reach; }

	void OnRegisterSceneNode() override;
	void render() override;
	const aabb3f &getBoundingBox() const override { return m_box; }

	// Rereads the Horizon settings (they're watched).
	void readSettings();

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
		aabb3f box{ v3f(0) }; // its parts', in the tile's frame (scene units)
		bool ready = false;
		bool queued = false;
		u32 used = 0; // the frame it was last wanted or drawn
	};

	// the quadtree for this frame: which tiles to draw, falling back to what's ready
	bool choose(const Key &key, std::vector<Key> &draw, std::vector<std::pair<float, Key>> &want);
	void install();
	void updateMask(ClientMap &map, v3f origin);
	void evict();

	Horizon m_horizon;
	std::unique_ptr<Layout> m_layout; // the planet's
	std::unique_ptr<Colours> m_colours;
	std::unique_ptr<Workers> m_workers;
	std::shared_ptr<DrawState> m_state;
	irr_ptr<Uniforms> m_uniforms;
	video::SMaterial m_material;
	video::ITexture *m_mask = nullptr;
	aabb3f m_box{ -1e7f, -1e7f, -1e7f, 1e7f, 1e7f, 1e7f };

	// settings
	bool m_enabled = true;
	float m_split = 2; // a tile splits while nearer than this many of its spans past the cut
	float m_max = 0;

	std::map<Key, Entry> m_tiles;
	std::unordered_map<u32, Surface> m_surfaces; // by map-block column (columnKey)
	std::vector<Key> m_draw;
	u32 m_frame = 0;
	float m_cut = 0, m_range = 0; // blocks
	float m_reach = 0;            // scene units
	bool m_showing = false;
	float m_altitude = 0;         // m above the sea
	float m_share = 0;            // 0 in the air's haze .. 1 above the air
	float m_lift = 0;             // m: the height that counts toward a tile's distance
	v3s16 m_camera_offset;
	std::vector<u8> m_near; // the mask's r on this side: the near world draws all of the column
	ClientMap *m_map = nullptr; // the near world, as of the last update()
	v3f m_eye;                  // where it's drawn from then, in blocks
};

} // namespace far
