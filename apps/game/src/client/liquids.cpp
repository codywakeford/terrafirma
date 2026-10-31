// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "client/liquids.h"
#include "client/clientmap.h"
#include "fill/fill.h"
#include "light.h"
#include "mapblock.h"
#include "nodedef.h"
#include "shape/shape.h"
#include <CMeshBuffer.h>
#include <ISceneManager.h>
#include <IVideoDriver.h>
#include <algorithm>
#include <cmath>

namespace liquids {

static const float GLIDE = 4;      // per s: how fast a shown level closes on its fill (a quarter-second's ease)
static const float SETTLED = 0.002; // of its room: close enough to stop gliding
static const u8 ALPHA = 175;
static const float RELIGHT = 2;    // s between drawing again by the day's light

Layer::Layer(scene::ISceneManager *smgr, const NodeDefManager *ndef) :
	scene::ISceneNode(smgr->getRootSceneNode(), smgr, -1), m_ndef(ndef)
{
	m_material.MaterialType = video::EMT_TRANSPARENT_VERTEX_ALPHA;
	m_material.BackfaceCulling = false;
	m_material.ZWriteEnable = video::EZW_OFF;
	m_material.FogEnable = true;
}

Layer::~Layer()
{
	for (auto &[pos, b] : m_blocks)
		if (b.mesh)
			b.mesh->drop();
}

// A block's fills, as they are now: new ones shown at once (a block just
// arrived), changed ones gliding from where they were shown
void Layer::take(v3s16 blockpos, ClientMap &map)
{
	MapBlock *block = map.getBlockNoCreateNoEx(blockpos);
	Block &b = m_blocks[blockpos];
	std::unordered_map<u16, Shown> was;
	was.swap(b.nodes);
	if (!block)
		return;
	block->m_fills.each([&](v3s16 rel, const fill::Fill &f) {
		const u16 k = u16((rel.Z * MAP_BLOCKSIZE + rel.Y) * MAP_BLOCKSIZE + rel.X);
		const float target = f.amount / 255.0f;
		auto old = was.find(k);
		Shown s{ old != was.end() ? old->second.level : target, target, f.r, f.g, f.b };
		b.nodes[k] = s;
		if (std::fabs(s.level - s.target) > SETTLED)
			b.gliding = true;
	});
	// what's gone drains away
	for (const auto &[k, s] : was)
		if (!b.nodes.count(k) && s.level > SETTLED) {
			b.nodes[k] = Shown{ s.level, 0, s.r, s.g, s.b };
			b.gliding = true;
		}
}

void Layer::update(const std::unordered_set<v3s16> &changed, ClientMap &map, v3s16 offset, u32 daynight, float dtime)
{
	m_offset = offset;
	std::unordered_set<v3s16> rebuild;
	for (v3s16 bp : changed) {
		take(bp, map);
		rebuild.insert(bp);
		// a neighbour's sides face this one's liquid
		for (v3s16 d : { v3s16(1, 0, 0), v3s16(-1, 0, 0), v3s16(0, 0, 1), v3s16(0, 0, -1) })
			if (m_blocks.count(bp + d))
				rebuild.insert(bp + d);
	}
	m_relight -= dtime;
	const bool relight = m_relight <= 0;
	if (relight)
		m_relight = RELIGHT;
	const float step = std::min(1.0f, GLIDE * dtime);
	for (auto it = m_blocks.begin(); it != m_blocks.end();) {
		Block &b = it->second;
		// unloaded: let it go
		if (!map.getBlockNoCreateNoEx(it->first)) {
			if (b.mesh)
				b.mesh->drop();
			it = m_blocks.erase(it);
			continue;
		}
		if (b.gliding) {
			b.gliding = false;
			for (auto n = b.nodes.begin(); n != b.nodes.end();) {
				Shown &s = n->second;
				s.level += (s.target - s.level) * step;
				if (std::fabs(s.target - s.level) <= SETTLED)
					s.level = s.target;
				else
					b.gliding = true;
				if (s.level <= 0 && s.target <= 0)
					n = b.nodes.erase(n);
				else
					++n;
			}
			rebuild.insert(it->first);
		}
		if (relight && !b.nodes.empty())
			rebuild.insert(it->first);
		++it;
	}
	for (v3s16 bp : rebuild) {
		auto it = m_blocks.find(bp);
		if (it != m_blocks.end())
			build(bp, it->second, map, daynight);
	}
}

// Where the liquid in a node stands, in its height (0..1): from its shown
// level when it's one of ours, else its fill as kept; −1: none
static float standing(const fill::Room &room, float level)
{
	return level > 0 ? room.surface(level) : -1;
}

void Layer::build(v3s16 blockpos, Block &b, ClientMap &map, u32 daynight)
{
	if (!b.mesh) {
		b.mesh = new scene::SMeshBuffer();
		b.mesh->setHardwareMappingHint(scene::EHM_STREAM);
	}
	auto &vertices = b.mesh->Vertices->Data;
	auto &indices = b.mesh->Indices->Data;
	vertices.clear();
	indices.clear();
	const v3s16 base = blockpos * MAP_BLOCKSIZE;
	// a quad, its corners in node units from the block's origin, facing `normal`
	auto quad = [&](v3f a, v3f c1, v3f c2, v3f d, v3f normal, video::SColor colour) {
		const u16 at = u16(vertices.size());
		for (const v3f &p : { a, c1, c2, d })
			vertices.emplace_back(p * BS, normal, colour, v2f(0, 0));
		for (u16 i : { 0, 1, 2, 2, 3, 0 })
			indices.push_back(at + i);
	};
	// a neighbour's liquid height (node units, from its floor), −1: none, 2: walled (solid there)
	auto beside = [&](v3s16 p) -> float {
		const MapNode n = map.getNode(p);
		if (n.getContent() == CONTENT_IGNORE)
			return 2;
		const ContentFeatures &f = m_ndef->get(n);
		if (f.drawtype == NDT_LIQUID)
			return 1; // the sea's own water
		const std::optional<shape::Shape> s = map.solidShape(p);
		if (!s && f.walkable && f.param_type_2 != CPT2_LEVELED)
			return 2;
		const fill::Fill *there = map.getFill(p);
		if (!there)
			return -1;
		return fill::Room::of(s ? &*s : nullptr).surface(there->amount / 255.0f);
	};
	for (const auto &[k, s] : b.nodes) {
		if (s.level <= 0)
			continue;
		const v3s16 rel(k % MAP_BLOCKSIZE, (k / MAP_BLOCKSIZE) % MAP_BLOCKSIZE, k / (MAP_BLOCKSIZE * MAP_BLOCKSIZE));
		const v3s16 p = base + rel;
		const std::optional<shape::Shape> solid = map.solidShape(p); // cut, or smoothed ground
		const shape::Shape *cut = solid ? &*solid : nullptr;
		const fill::Room room = fill::Room::of(cut);
		const float h = standing(room, s.level);
		if (h <= 0)
			continue;
		// lit as the node it's in, by the day
		const MapNode node = map.getNode(p);
		const u8 light = decode_light(node.getLightBlend(daynight, m_ndef->getLightingFlags(node)));
		const float shade = 0.25f + 0.75f * light / 255.0f;
		const video::SColor colour(ALPHA, u32(s.r * shade), u32(s.g * shade), u32(s.b * shade));
		const v3f o(rel.X - 0.5f, rel.Y - 0.5f, rel.Z - 0.5f);
		const float top = o.Y + h;
		// the surface: whole, or over the open sixteenths of the layer it stands in
		const bool covered = h >= 0.999f && beside(p + v3s16(0, 1, 0)) >= 0 && beside(p + v3s16(0, 1, 0)) <= 1;
		if (!covered) {
			if (!cut) {
				quad(v3f(o.X, top, o.Z), v3f(o.X, top, o.Z + 1), v3f(o.X + 1, top, o.Z + 1), v3f(o.X + 1, top, o.Z),
						v3f(0, 1, 0), colour);
			} else {
				const int layer = std::clamp(int(std::ceil(h * 16)) - 1, 0, 15);
				for (int z = 0; z < 16; z++)
					for (int x = 0; x < 16;) {
						if (cut->has(v3s16(x, layer, z))) {
							x++;
							continue;
						}
						int end = x;
						while (end < 16 && !cut->has(v3s16(end, layer, z)))
							end++;
						const float x0 = o.X + x / 16.0f, x1 = o.X + end / 16.0f;
						const float z0 = o.Z + z / 16.0f, z1 = o.Z + (z + 1) / 16.0f;
						quad(v3f(x0, top, z0), v3f(x0, top, z1), v3f(x1, top, z1), v3f(x1, top, z0), v3f(0, 1, 0), colour);
						x = end;
					}
			}
		}
		// the sides: where it stands above what's beside it (open air, a lower fill)
		static const v3s16 SIDES[4] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };
		for (const v3s16 &d : SIDES) {
			const float there = beside(p + d);
			if (there >= 2 || there >= h)
				continue;
			const float from = o.Y + std::max(0.0f, there), to = top;
			const v3f n(d.X, 0, d.Z);
			if (d.X) {
				const float x = d.X > 0 ? o.X + 1 : o.X;
				quad(v3f(x, from, o.Z), v3f(x, to, o.Z), v3f(x, to, o.Z + 1), v3f(x, from, o.Z + 1), n, colour);
			} else {
				const float z = d.Z > 0 ? o.Z + 1 : o.Z;
				quad(v3f(o.X, from, z), v3f(o.X, to, z), v3f(o.X + 1, to, z), v3f(o.X + 1, from, z), n, colour);
			}
		}
	}
	b.mesh->recalculateBoundingBox();
	b.mesh->setDirty();
}

void Layer::OnRegisterSceneNode()
{
	if (IsVisible && !m_blocks.empty())
		SceneManager->registerNodeForRendering(this, scene::ESNRP_TRANSPARENT);
	ISceneNode::OnRegisterSceneNode();
}

void Layer::render()
{
	video::IVideoDriver *driver = SceneManager->getVideoDriver();
	if (SceneManager->getSceneNodeRenderPass() != scene::ESNRP_TRANSPARENT)
		return;
	driver->setMaterial(m_material);
	for (const auto &[bp, b] : m_blocks) {
		if (!b.mesh || b.mesh->getIndexCount() == 0)
			continue;
		core::matrix4 world;
		world.setTranslation(intToFloat(bp * MAP_BLOCKSIZE - m_offset, BS));
		driver->setTransform(video::ETS_WORLD, world);
		driver->drawMeshBuffer(b.mesh);
	}
}

} // namespace liquids
