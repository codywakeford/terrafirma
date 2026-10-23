// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "far/terrain.h"
#include "irr_ptr.h"
#include "irrlichttypes_bloated.h"
#include <CMeshBuffer.h>
#include <ISceneNode.h>
#include <SMaterial.h>
#include <map>
#include <unordered_map>
#include <memory>
#include <optional>
#include <vector>

class Camera;
class ClientMap;
class IShaderSource;
class ITextureSource;
class NodeDefManager;
namespace video { class ITexture; }

namespace far {

class Colours;

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

	Drawn right after the sky with a projection of its own (kilometres of it
	don't cost the near world depth precision), then the depth buffer is
	cleared. The planet's curve is applied as it's drawn, never stored.
*/
class View : public scene::ISceneNode
{
public:
	View(scene::ISceneManager *smgr, IShaderSource *ssrc, ITextureSource *tsrc,
			const NodeDefManager *ndef, const Horizon &horizon);
	~View();

	// Once a frame, before drawing. `near_range`: how far the map blocks are
	// drawn, in blocks. `fog_limit`: a server's fog distance, in blocks.
	void update(const Camera &camera, ClientMap &map, float near_range, bool range_all,
			std::optional<float> fog_limit);

	// Whether it draws this frame, and how far it reaches (blocks): the fog
	// follows it.
	bool showing() const { return m_showing; }
	float range() const { return m_range; }

	void OnRegisterSceneNode() override;
	void render() override;
	const aabb3f &getBoundingBox() const override { return m_box; }

	// Rereads the Horizon settings (they're watched).
	void readSettings();

private:
	struct Key
	{
		s32 level, x, z;
		bool operator<(const Key &o) const
		{
			return level != o.level ? level < o.level : x != o.x ? x < o.x : z < o.z;
		}
		TileKey tile() const { return { level, x, z }; }
	};
	struct Entry
	{
		std::vector<irr_ptr<scene::SMeshBuffer>> parts;
		bool ready = false;
		bool queued = false;
		u32 used = 0; // the frame it was last wanted or drawn
	};

	// the quadtree for this frame: which tiles to draw, falling back to what's ready
	bool choose(const Key &key, v2f eye, std::vector<Key> &draw, std::vector<std::pair<float, Key>> &want);
	float distance(const Key &key, v2f eye) const;
	void install();
	void updateMask(ClientMap &map, v3f eye);
	void evict();

	Horizon m_horizon;
	float m_radius; // blocks
	s32 m_size;
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
	float m_cut = 0, m_range = 0;
	bool m_showing = false;
	v3s16 m_camera_offset;
	v3s16 m_mask_centre; // map block the mask is centred on
	u32 m_mask_frame = 0;
};

} // namespace far
