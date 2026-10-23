// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "view.h"
#include "far/mesh.h"
#include "client/camera.h"
#include "client/clientmap.h"
#include "client/shader.h"
#include "client/texturesource.h"
#include "constants.h"
#include "exceptions.h"
#include "log.h"
#include "mapblock.h"
#include "nodedef.h"
#include "settings.h"
#include <ICameraSceneNode.h>
#include <SViewFrustum.h>
#include <ISceneManager.h>
#include <IVideoDriver.h>
#include <algorithm>
#include <cmath>
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <set>
#include <thread>
#include <tuple>

namespace far {

// A tile splits while it's nearer than this many of its own spans past the
// near world's edge, by the Horizon detail setting (1..4).
static const float SPLIT[4] = { 1.0f, 1.5f, 2.0f, 3.0f };
// The coarsest level: 32 · 2⁹ = 16 384 blocks, the largest world.
static constexpr s32 TOP_LEVEL = 9;
// Tiles put on the GPU per frame, at most.
static constexpr int INSTALLS_PER_FRAME = 6;
// A tile not wanted for this many frames is dropped.
static constexpr u32 KEEP_FRAMES = 600;
// The mask: map-block columns around the camera, a texel each.
static constexpr s32 MASK = 128;

static float span(s32 level)
{
	return (float)(TILE << level);
}

// What the shader needs while the far view draws (scene units).
struct DrawState
{
	v3f eye;
	float radius = 1;
	float cut = 0;
	float haze = 1;     // distance past the cut over which the air's haze builds
	float mask[4] = {}; // scene x, z of the mask's corner, its size, 1 if it applies
};

class Uniforms : public IShaderUniformSetterRC
{
public:
	explicit Uniforms(std::shared_ptr<const DrawState> state) : m_state(std::move(state)) {}

	void onSetUniforms(video::IMaterialRendererServices *services) override
	{
		m_eye_vertex.set(m_state->eye, services);
		m_eye_pixel.set(m_state->eye, services);
		m_radius.set(&m_state->radius, services);
		m_cut_vertex.set(&m_state->cut, services);
		m_cut_pixel.set(&m_state->cut, services);
		m_haze.set(&m_state->haze, services);
		m_mask.set(m_state->mask, services);
	}

private:
	std::shared_ptr<const DrawState> m_state;
	CachedVertexShaderSetting<float, 3> m_eye_vertex{"farEye"};
	CachedPixelShaderSetting<float, 3> m_eye_pixel{"farEye"};
	CachedVertexShaderSetting<float> m_radius{"farRadius"};
	CachedVertexShaderSetting<float> m_cut_vertex{"farCut"};
	CachedPixelShaderSetting<float> m_cut_pixel{"farCut"};
	CachedPixelShaderSetting<float> m_haze{"farHaze"};
	CachedPixelShaderSetting<float, 4> m_mask{"farMask"};
};

// From lowest ground to the top of the highest water or tree, per map-block
// column a tile covers: what the near world must draw before the far view
// can step aside there. Tiles coarser than a map block don't say.
static std::vector<Surface> surfaces(const Tile &tile)
{
	std::vector<Surface> out;
	const s32 cell = tile.key.cell();
	if (cell > MAP_BLOCKSIZE)
		return out;
	const s32 per = MAP_BLOCKSIZE / cell, columns = TILE / per;
	const s32 bx0 = tile.key.x * tile.key.span() / MAP_BLOCKSIZE, bz0 = tile.key.z * tile.key.span() / MAP_BLOCKSIZE;
	for (s32 bj = 0; bj < columns; bj++)
	for (s32 bi = 0; bi < columns; bi++) {
		Surface s{ (s16)(bx0 + bi), (s16)(bz0 + bj), INT32_MAX, INT32_MIN, (u8)tile.key.level };
		for (s32 j = bj * per; j < (bj + 1) * per; j++)
		for (s32 i = bi * per; i < (bi + 1) * per; i++) {
			const Cell &c = tile.at(i, j);
			s.low = std::min(s.low, c.ground);
			s.high = std::max({ s.high, c.ground, c.water, c.canopy.empty() ? c.ground : c.canopy.high });
		}
		out.push_back(s);
	}
	return out;
}

/*
	The workers: build tiles and their meshes off the main thread, nearest
	first. The view says each frame which tiles it wants (want()); keys being
	built or waiting to be installed aren't asked for twice.
*/
class Workers
{
public:
	struct Result
	{
		TileKey key;
		Mesh mesh;
		std::vector<Surface> surfaces;
	};

	Workers(const Horizon &horizon, const Colours &colours) : m_horizon(horizon), m_colours(colours)
	{
		unsigned n = std::clamp(std::thread::hardware_concurrency() / 4, 1u, 3u);
		for (unsigned i = 0; i < n; i++)
			m_threads.emplace_back([this] { run(); });
	}

	~Workers()
	{
		{
			std::lock_guard lock(m_mutex);
			m_stop = true;
		}
		m_wake.notify_all();
		for (std::thread &t : m_threads)
			t.join();
	}

	// the tiles wanted, nearest first; replaces what was asked before
	void want(const std::vector<std::pair<float, TileKey>> &keys)
	{
		{
			std::lock_guard lock(m_mutex);
			m_queue.clear();
			for (const auto &k : keys)
				if (!m_busy.count(id(k.second)))
					m_queue.push_back(k.second);
		}
		m_wake.notify_all();
	}

	std::optional<Result> take()
	{
		std::lock_guard lock(m_mutex);
		if (!m_error.empty())
			throw BaseException("far view: " + m_error);
		if (m_results.empty())
			return std::nullopt;
		Result r = std::move(m_results.front());
		m_results.pop_front();
		m_busy.erase(id(r.key));
		return r;
	}

private:
	static std::tuple<s32, s32, s32> id(const TileKey &k) { return { k.level, k.x, k.z }; }

	void run()
	{
		{
			// the first worker in builds the generator (the world model: a moment)
			std::lock_guard lock(m_mutex);
			if (!m_generator && m_error.empty()) {
				try {
					m_generator = std::make_unique<Generator>(m_horizon);
				} catch (const std::exception &e) {
					m_error = std::string("the world can't be built from the server's data: ") + e.what();
				}
			}
		}
		m_wake.notify_all();
		std::unique_lock lock(m_mutex);
		while (true) {
			m_wake.wait(lock, [this] { return m_stop || (!m_queue.empty() && m_generator); });
			if (m_stop)
				return;
			TileKey key = m_queue.front();
			m_queue.pop_front();
			m_busy.insert(id(key));
			lock.unlock();

			Result r{ key, {}, {} };
			try {
				Tile tile = m_generator->build(key);
				r.mesh = mesh(tile, m_colours);
				r.surfaces = surfaces(tile);
			} catch (const std::exception &e) {
				lock.lock();
				m_error = e.what();
				return;
			}

			lock.lock();
			m_results.push_back(std::move(r));
		}
	}

	const Horizon &m_horizon;
	const Colours &m_colours;
	std::unique_ptr<Generator> m_generator;

	std::mutex m_mutex;
	std::condition_variable m_wake;
	bool m_stop = false;
	std::deque<TileKey> m_queue;
	std::set<std::tuple<s32, s32, s32>> m_busy; // being built, or built and not yet taken
	std::deque<Result> m_results;
	std::string m_error;
	std::vector<std::thread> m_threads; // last: started once everything above exists
};

static void settingChanged(const std::string &, void *data)
{
	static_cast<View *>(data)->readSettings();
}

static const char *const SETTINGS[] = { "far_view", "far_view_detail", "far_view_max" };

View::View(scene::ISceneManager *smgr, IShaderSource *ssrc, ITextureSource *tsrc,
		const NodeDefManager *ndef, const Horizon &horizon) :
	scene::ISceneNode(smgr->getRootSceneNode(), smgr, -1),
	m_horizon(horizon),
	m_state(std::make_shared<DrawState>())
{
	terragen::WorldCode code = terragen::WorldCode::decode(horizon.code);
	m_radius = radius(code);
	m_size = code.size;
	m_colours = std::make_unique<Colours>(ndef, tsrc, blocksIn(m_horizon));
	m_workers = std::make_unique<Workers>(m_horizon, *m_colours);

	m_uniforms = make_irr<Uniforms>(m_state);
	u32 shader = ssrc->getShader("far_shader", ShaderConstants{}, video::EMT_SOLID, m_uniforms.get());
	m_material.MaterialType = ssrc->getShaderInfo(shader).material;
	m_material.BackfaceCulling = false;
	m_material.FogEnable = false; // the shader fogs to the horizon itself
	m_material.UseMipMaps = false;

	// the mask: rewritten as the near world loads, so no mipmaps to go stale
	video::IVideoDriver *driver = smgr->getVideoDriver();
	bool mipmaps = driver->getTextureCreationFlag(video::ETCF_CREATE_MIP_MAPS);
	driver->setTextureCreationFlag(video::ETCF_CREATE_MIP_MAPS, false);
	m_mask = driver->addTexture(core::dimension2du(MASK, MASK), "far_view_mask", video::ECF_A8R8G8B8);
	driver->setTextureCreationFlag(video::ETCF_CREATE_MIP_MAPS, mipmaps);
	if (!m_mask)
		throw BaseException("far view: can't make its mask texture");
	auto &layer = m_material.TextureLayers[0];
	layer.Texture = m_mask;
	layer.MinFilter = video::ETMINF_NEAREST_MIPMAP_NEAREST;
	layer.MagFilter = video::ETMAGF_NEAREST;
	layer.TextureWrapU = layer.TextureWrapV = video::ETC_CLAMP_TO_EDGE;

	// drawn whatever the camera looks at: the tiles surround it
	setAutomaticCulling(scene::EAC_OFF);

	readSettings();
	for (const char *name : SETTINGS)
		g_settings->registerChangedCallback(name, &settingChanged, this);
	infostream << "far view: " << horizon.code << ", horizon radius " << m_radius << " blocks, "
			<< horizon.species.size() << " tree species" << std::endl;
}

View::~View()
{
	g_settings->deregisterAllChangedCallbacks(this);
	m_workers.reset(); // before the colours they read
	if (m_mask)
		SceneManager->getVideoDriver()->removeTexture(m_mask);
}

// whether the box is wholly outside one of the frustum's planes
static bool outside(const scene::SViewFrustum &f, const aabb3f &box)
{
	v3f corners[8];
	box.getEdges(corners);
	for (const auto &plane : f.planes) {
		bool all = true;
		for (const v3f &c : corners)
			if (plane.classifyPointRelation(c) != core::ISREL3D_FRONT) {
				all = false;
				break;
			}
		if (all)
			return true;
	}
	return false;
}

void View::readSettings()
{
	m_enabled = g_settings->getBool("far_view");
	int detail = std::clamp((int)std::lround(g_settings->getFloat("far_view_detail")), 1, 4);
	m_split = SPLIT[detail - 1];
	m_max = g_settings->getFloat("far_view_max");
}

float View::distance(const Key &key, v2f eye) const
{
	float s = span(key.level), x0 = key.x * s - 0.5f, z0 = key.z * s - 0.5f;
	float dx = std::max({ x0 - eye.X, 0.0f, eye.X - (x0 + s) });
	float dz = std::max({ z0 - eye.Y, 0.0f, eye.Y - (z0 + s) });
	return std::sqrt(dx * dx + dz * dz);
}

bool View::choose(const Key &key, v2f eye, std::vector<Key> &draw, std::vector<std::pair<float, Key>> &want)
{
	float d = distance(key, eye);
	if (d > m_range)
		return true; // beyond the horizon's reach: nothing to draw
	bool split = key.level > 0 && d - m_cut < span(key.level) * m_split;

	if (!split) {
		Entry &e = m_tiles[key];
		e.used = m_frame;
		if (e.ready) {
			draw.push_back(key);
			return true;
		}
		want.emplace_back(d, key);
		// meanwhile, finer tiles from before, if all of them are there
		size_t mark = draw.size();
		std::function<bool(const Key &)> finer = [&](const Key &k) {
			if (k.level == 0)
				return false;
			for (s32 c = 0; c < 4; c++) {
				Key child{ k.level - 1, k.x * 2 + (c & 1), k.z * 2 + (c >> 1) };
				auto it = m_tiles.find(child);
				if (it != m_tiles.end() && it->second.ready) {
					it->second.used = m_frame;
					draw.push_back(child);
				} else if (!finer(child)) {
					return false;
				}
			}
			return true;
		};
		if (finer(key))
			return true;
		draw.resize(mark);
		return false;
	}

	size_t mark = draw.size();
	bool all = true;
	for (s32 c = 0; c < 4; c++)
		all &= choose({ key.level - 1, key.x * 2 + (c & 1), key.z * 2 + (c >> 1) }, eye, draw, want);
	if (all)
		return true;
	// a finer tile isn't there yet: this one stands in for all four
	auto it = m_tiles.find(key);
	if (it != m_tiles.end() && it->second.ready) {
		draw.resize(mark);
		draw.push_back(key);
		it->second.used = m_frame;
		return true;
	}
	return false;
}

void View::update(const Camera &camera, ClientMap &map, float near_range, bool range_all,
		std::optional<float> fog_limit)
{
	m_frame++;
	m_camera_offset = camera.getOffset();
	// beyond half the world east–west lies the same land again
	float range = std::min(m_max, (float)m_size / 2);
	if (fog_limit)
		range = std::min(range, *fog_limit);
	m_cut = near_range;
	m_range = range;
	m_showing = m_enabled && !range_all && range > near_range;
	if (!m_showing)
		return;

	v3f eye = (camera.getCameraNode()->getPosition() + intToFloat(m_camera_offset, BS)) / BS;
	v2f flat(eye.X, eye.Z);

	// the coarsest tiles that reach the range, split down toward the camera
	m_draw.clear();
	std::vector<std::pair<float, Key>> want;
	s32 top = 0;
	while (top < TOP_LEVEL && span(top) < range / 2)
		top++;
	s32 lo_x = (s32)std::floor((flat.X - range) / span(top)), hi_x = (s32)std::floor((flat.X + range) / span(top));
	s32 lo_z = (s32)std::floor((flat.Y - range) / span(top)), hi_z = (s32)std::floor((flat.Y + range) / span(top));
	for (s32 z = lo_z; z <= hi_z; z++)
		for (s32 x = lo_x; x <= hi_x; x++)
			choose({ top, x, z }, flat, m_draw, want);

	std::sort(want.begin(), want.end(), [](const auto &a, const auto &b) { return a.first < b.first; });
	std::vector<std::pair<float, TileKey>> keys;
	keys.reserve(want.size());
	for (const auto &w : want)
		keys.emplace_back(w.first, w.second.tile());
	m_workers->want(keys);

	install();
	m_map = &map;
	m_eye = eye;
	if (m_frame % 60 == 0)
		evict();
}

void View::install()
{
	for (int n = 0; n < INSTALLS_PER_FRAME; n++) {
		auto result = m_workers->take();
		if (!result)
			return;
		const TileKey &k = result->key;
		for (const Surface &s : result->surfaces) {
			auto [it, fresh] = m_surfaces.emplace(columnKey(s.bx, s.bz), s);
			if (!fresh && s.level <= it->second.level)
				it->second = s; // the finer tile knows better
		}
		Entry &e = m_tiles[{ k.level, k.x, k.z }];
		e.parts.clear();
		for (Mesh::Part &part : result->mesh.parts) {
			auto buffer = make_irr<scene::SMeshBuffer>();
			buffer->Vertices->Data = std::move(part.vertices);
			buffer->Indices->Data = std::move(part.indices);
			buffer->recalculateBoundingBox();
			buffer->setHardwareMappingHint(scene::EHM_STATIC);
			if (e.parts.empty())
				e.box = buffer->getBoundingBox();
			else
				e.box.addInternalBox(buffer->getBoundingBox());
			e.parts.push_back(std::move(buffer));
		}
		e.ready = true;
	}
}

void View::updateMask(ClientMap &map, v3f eye)
{
	m_mask_centre = getNodeBlockPos(floatToInt(eye * BS, BS));
	const s32 half = MASK / 2;
	// the map blocks the near world draws this frame, by column
	std::vector<std::vector<s16>> drawn((size_t)MASK * MASK);
	map.forEachDrawn([&](v3s16 block) {
		s32 u = block.X - m_mask_centre.X + half, v = block.Z - m_mask_centre.Z + half;
		if (u >= 0 && u < MASK && v >= 0 && v < MASK)
			drawn[(size_t)v * MASK + u].push_back(block.Y);
	});
	auto *pixels = static_cast<u32 *>(m_mask->lock(video::ETLM_WRITE_ONLY, 0, 0, video::ETLF_NONE));
	if (!pixels)
		throw BaseException("far view: can't write its mask texture");
	m_near.assign((size_t)MASK * MASK, 0);
	for (s32 v = 0; v < MASK; v++)
	for (s32 u = 0; u < MASK; u++) {
		std::vector<s16> &ys = drawn[(size_t)v * MASK + u];
		// The near world stands in for a column only if it draws all of its
		// surface: at its edge it draws by distance, and may have the seabed
		// or a hill's foot without the top. Where it draws the top but not
		// the floor (low over the sea), its water lies over our floor.
		bool near = !ys.empty(), top = near;
		auto it = m_surfaces.find(columnKey(m_mask_centre.X - half + u, m_mask_centre.Z - half + v));
		if (near && it != m_surfaces.end()) {
			std::sort(ys.begin(), ys.end());
			s16 lo = getContainerPos(it->second.low, MAP_BLOCKSIZE), hi = getContainerPos(it->second.high, MAP_BLOCKSIZE);
			top = std::binary_search(ys.begin(), ys.end(), hi);
			for (s16 y = lo; y <= hi && near; y++)
				near = std::binary_search(ys.begin(), ys.end(), y);
		}
		m_near[(size_t)v * MASK + u] = near;
		pixels[(size_t)v * MASK + u] = video::SColor(255, near ? 255 : 0, top ? 255 : 0, 0).color;
	}
	m_mask->unlock();
}

void View::evict()
{
	// surfaces of columns long out of range
	v2f eye(m_mask_centre.X, m_mask_centre.Z);
	float keep = (m_range * 1.5f) / MAP_BLOCKSIZE;
	for (auto it = m_surfaces.begin(); it != m_surfaces.end();) {
		if (std::fabs(it->second.bx - eye.X) > keep || std::fabs(it->second.bz - eye.Y) > keep)
			it = m_surfaces.erase(it);
		else
			++it;
	}
	for (auto it = m_tiles.begin(); it != m_tiles.end();) {
		if (m_frame - it->second.used > KEEP_FRAMES)
			it = m_tiles.erase(it);
		else
			++it;
	}
}

bool View::covered(const Key &key) const
{
	if (m_near.empty())
		return false;
	s32 s = TILE << key.level, half = MASK / 2;
	s32 bx0 = getContainerPos(key.x * s, MAP_BLOCKSIZE), bx1 = getContainerPos(key.x * s + s - 1, MAP_BLOCKSIZE);
	s32 bz0 = getContainerPos(key.z * s, MAP_BLOCKSIZE), bz1 = getContainerPos(key.z * s + s - 1, MAP_BLOCKSIZE);
	for (s32 bz = bz0; bz <= bz1; bz++)
	for (s32 bx = bx0; bx <= bx1; bx++) {
		s32 u = bx - m_mask_centre.X + half, v = bz - m_mask_centre.Z + half;
		if (u < 0 || u >= MASK || v < 0 || v >= MASK || !m_near[(size_t)v * MASK + u])
			return false;
	}
	return true;
}

void View::OnRegisterSceneNode()
{
	if (IsVisible && m_showing)
		SceneManager->registerNodeForRendering(this, scene::ESNRP_SKY_BOX);
	ISceneNode::OnRegisterSceneNode();
}

void View::render()
{
	video::IVideoDriver *driver = SceneManager->getVideoDriver();
	scene::ICameraSceneNode *camera = SceneManager->getActiveCamera();
	if (!camera || SceneManager->getSceneNodeRenderPass() != scene::ESNRP_SKY_BOX)
		return;

	// the mask, from exactly the blocks the near world is about to draw
	// (its draw list is updated after our update(), and not every frame)
	updateMask(*m_map, m_eye);

	v3f eye = camera->getAbsolutePosition();
	v3f offset = intToFloat(m_camera_offset, BS);
	m_state->eye = eye;
	m_state->radius = m_radius * BS;
	m_state->cut = m_cut * BS;
	m_state->haze = std::max(m_range - m_cut, 1.0f) * BS;
	m_state->mask[0] = ((float)(m_mask_centre.X - MASK / 2) * MAP_BLOCKSIZE - 0.5f) * BS - offset.X;
	m_state->mask[1] = ((float)(m_mask_centre.Z - MASK / 2) * MAP_BLOCKSIZE - 0.5f) * BS - offset.Z;
	m_state->mask[2] = (float)(MAP_BLOCKSIZE * MASK) * BS;
	m_state->mask[3] = 1;

	// its own depth range: from a block away to past the horizon
	float altitude = std::fabs((eye.Y + offset.Y) / BS - m_horizon.water_level);
	float zfar = (m_range * 1.5f + altitude + POLAR_WALL) * BS;
	core::matrix4 projection = driver->getTransform(video::ETS_PROJECTION);
	core::matrix4 own;
	own.buildProjectionMatrixPerspectiveFovLH(camera->getFOV(), camera->getAspectRatio(),
			BS, zfar, false);
	driver->setTransform(video::ETS_PROJECTION, own);
	driver->setMaterial(m_material);
	scene::SViewFrustum frustum(own * driver->getTransform(video::ETS_VIEW), false);

	for (const Key &key : m_draw) {
		auto it = m_tiles.find(key);
		if (it == m_tiles.end())
			continue;
		// the near world draws all of it: nothing of ours would show
		if (key.level <= 3 && covered(key))
			continue;
		float s = span(key.level);
		v3f at(key.x * s * BS - offset.X, -offset.Y, key.z * s * BS - offset.Z);
		// out of view (allowing for the curve, which only lowers it)
		aabb3f box(it->second.box.MinEdge + at, it->second.box.MaxEdge + at);
		float dx = std::max(std::fabs(box.MinEdge.X - eye.X), std::fabs(box.MaxEdge.X - eye.X));
		float dz = std::max(std::fabs(box.MinEdge.Z - eye.Z), std::fabs(box.MaxEdge.Z - eye.Z));
		float past = std::max(std::sqrt(dx * dx + dz * dz) - m_state->cut, 0.0f);
		box.MinEdge.Y -= past * (past + 2 * m_state->cut) / (2 * m_state->radius);
		if (outside(frustum, box))
			continue;
		core::matrix4 world;
		world.setTranslation(at);
		driver->setTransform(video::ETS_WORLD, world);
		for (const auto &part : it->second.parts)
			driver->drawMeshBuffer(part.get());
	}

	driver->setTransform(video::ETS_PROJECTION, projection);
	// the near world draws over all of it
	driver->clearZBuffer();
}

} // namespace far
