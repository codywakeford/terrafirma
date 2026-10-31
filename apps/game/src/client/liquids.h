// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "irr_aabb3d.h"
#include "irr_v3d.h"
#include "irrlichttypes.h"
#include <CMeshBuffer.h>
#include <ISceneNode.h>
#include <SMaterial.h>
#include <unordered_map>
#include <unordered_set>

class ClientMap;
class NodeDefManager;

/*
	The liquid layer (sprint 81): liquid lying in the world's open space,
	drawn on its own, never by remeshing map blocks. Each node's fill
	(fill::Fill, kept by its map block) is drawn as a surface over the
	node's open columns at the height that much liquid stands at (a pit cut
	in sixteenths holds it to its shape) and sides where it stands above
	what's beside it, in its mixture's colour. A fill that changes glides to
	its new height, so water is seen to run, rise and drain.
*/
namespace liquids {

class Layer : public scene::ISceneNode
{
public:
	Layer(scene::ISceneManager *smgr, const NodeDefManager *ndef);
	~Layer() override;

	// A frame: the blocks whose fills changed (or arrived), the map, where
	// the scene's origin is (the camera offset, in nodes), and the time
	// since the last frame, for gliding.
	void update(const std::unordered_set<v3s16> &changed, ClientMap &map, v3s16 offset, u32 daynight, float dtime);

	void OnRegisterSceneNode() override;
	void render() override;
	const aabb3f &getBoundingBox() const override { return m_box; }

private:
	struct Shown
	{
		float level, target; // of its open space
		u8 r, g, b;
	};
	struct Block
	{
		std::unordered_map<u16, Shown> nodes; // by position in the block
		scene::SMeshBuffer *mesh = nullptr;
		bool gliding = false;
	};

	void take(v3s16 blockpos, ClientMap &map);
	void build(v3s16 blockpos, Block &b, ClientMap &map, u32 daynight);

	const NodeDefManager *m_ndef;
	std::unordered_map<v3s16, Block> m_blocks;
	v3s16 m_offset;
	video::SMaterial m_material;
	aabb3f m_box{ v3f(-1e7f), v3f(1e7f) }; // it is wherever liquid is: never culled as a whole
	float m_relight = 0; // s until the light's looked at again
};

} // namespace liquids
