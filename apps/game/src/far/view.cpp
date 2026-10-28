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
#include "terragen/planet.h"
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
// Tiles put on the GPU per frame, at most.
static constexpr int INSTALLS_PER_FRAME = 6;
// The air's depth, in its scale heights: above it the haze has given way
// to the planet seen from above (tf_flight's boundary is the same).
static constexpr double AIR_DEPTH = 8;
// A tile not wanted for this many frames is dropped.
static constexpr u32 KEEP_FRAMES = 600;

static float span(s32 level)
{
	return (float)(TILE << level);
}

// What the shader needs while the far view draws (scene units unless said).
struct DrawState
{
	float cut = 0;
	float haze = 1;     // distance past the cut over which the air's haze builds, near the ground
	float mask[4] = {}; // scene x, z of the mask's corner, its size, 1 if it applies
	float share = 0;    // 0 down in the air's haze .. 1 above the air, seeing the planet
	float sea = 0;      // the scene y of the sea
	float place[4] = {}; // the eye: map x, z (blocks), altitude (m), unused
	float air = 1;      // m: the air's scale height
};

class Uniforms : public IShaderUniformSetterRC
{
public:
	explicit Uniforms(std::shared_ptr<const DrawState> state) : m_state(std::move(state)) {}

	void onSetUniforms(video::IMaterialRendererServices *services) override
	{
		m_cut_vertex.set(&m_state->cut, services);
		m_cut_pixel.set(&m_state->cut, services);
		m_haze.set(&m_state->haze, services);
		m_mask.set(m_state->mask, services);
		m_share.set(&m_state->share, services);
		m_sea.set(&m_state->sea, services);
		m_place_vertex.set(m_state->place, services);
		m_place_pixel.set(m_state->place, services);
		m_air.set(&m_state->air, services);
	}

private:
	std::shared_ptr<const DrawState> m_state;
	CachedVertexShaderSetting<float> m_cut_vertex{"farCut"};
	CachedPixelShaderSetting<float> m_cut_pixel{"farCut"};
	CachedPixelShaderSetting<float> m_haze{"farHaze"};
	CachedPixelShaderSetting<float, 4> m_mask{"farMask"};
	CachedPixelShaderSetting<float> m_share{"farShare"};
	CachedVertexShaderSetting<float> m_sea{"farSea"};
	CachedVertexShaderSetting<float, 4> m_place_vertex{"farPlace"};
	CachedPixelShaderSetting<float, 4> m_place_pixel{"farPlace"};
	CachedPixelShaderSetting<float> m_air{"farAir"};
};

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

	Workers(const Horizon &horizon, const Colours &colours, const Layout &layout) :
		m_horizon(horizon), m_colours(colours), m_layout(layout)
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
	static std::tuple<s32, s32, s32, s32> id(const TileKey &k) { return { k.level, k.x, k.z, k.face }; }

	void run()
	{
		{
			// the first worker in builds the generator (the world model: a moment)
			std::lock_guard lock(m_mutex);
			if (!m_generator && m_error.empty()) {
				try {
					m_generator = m_layout.generator(m_horizon);
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
				r.mesh = m_layout.mesh(tile, m_colours);
				r.surfaces = m_layout.surfaces(tile);
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
	const Layout &m_layout;
	std::unique_ptr<Generator> m_generator;

	std::mutex m_mutex;
	std::condition_variable m_wake;
	bool m_stop = false;
	std::deque<TileKey> m_queue;
	std::set<std::tuple<s32, s32, s32, s32>> m_busy; // being built, or built and not yet taken
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
		const NodeDefManager *ndef, const Horizon &horizon, std::unique_ptr<Layout> layout) :
	scene::ISceneNode(smgr->getRootSceneNode(), smgr, -1),
	m_horizon(horizon),
	m_layout(std::move(layout)),
	m_state(std::make_shared<DrawState>())
{
	m_colours = std::make_unique<Colours>(ndef, tsrc, blocksIn(m_horizon));
	m_workers = std::make_unique<Workers>(m_horizon, *m_colours, *m_layout);

	m_uniforms = make_irr<Uniforms>(m_state);
	u32 shader = ssrc->getShader(m_layout->shader(), ShaderConstants{}, video::EMT_SOLID, m_uniforms.get());
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
	infostream << "far view: " << horizon.code << ", "
			<< horizon.species.size() << " tree species" << std::endl;
}

View::~View()
{
	g_settings->deregisterAllChangedCallbacks(this);
	m_workers.reset(); // before the colours they read
	if (m_mask)
		SceneManager->getVideoDriver()->removeTexture(m_mask);
}

void View::readSettings()
{
	m_enabled = g_settings->getBool("far_view");
	int detail = std::clamp((int)std::lround(g_settings->getFloat("far_view_detail")), 1, 4);
	m_split = SPLIT[detail - 1];
	m_max = g_settings->getFloat("far_view_max");
}

bool View::choose(const Key &key, std::vector<Key> &draw, std::vector<std::pair<float, Key>> &want)
{
	float d = m_layout->distance(key.tile());
	if (d > m_range)
		return true; // beyond the horizon's reach: nothing to draw
	// high up, a tile's size on screen goes by how far it is through the air
	float seen = std::sqrt(d * d + m_lift * m_lift);
	bool split = key.level > 0 && seen - m_cut < span(key.level) * m_split;

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
				const Key child = k.child(c);
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
		all &= choose(key.child(c), draw, want);
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
	v3f eye = (camera.getCameraNode()->getPosition() + intToFloat(m_camera_offset, BS)) / BS;
	m_layout->look(eye, map);
	m_altitude = m_layout->altitude();
	// up through the air the haze gives way to the planet seen from above
	const double r = m_layout->radius(), h = m_altitude;
	m_share = (float)std::clamp(h / (AIR_DEPTH * terragen::sphere(r).scale_height), 0.0, 1.0);
	m_lift = m_altitude; // a block is a metre: the height counts toward a tile's distance as is

	// out to the planet's horizon (at most half way round); how far a tile
	// is goes by the planet (the layout's distance), never by the map
	const float horizon = (float)std::sqrt(2 * r * h + h * h); // m
	float range = std::min(m_max, (float)(M_PI * r));
	if (fog_limit)
		range = std::min(range, *fog_limit);
	else
		range = std::min(std::max(range, 1.1f * horizon), (float)(M_PI * r));
	m_cut = near_range;
	m_range = range;
	m_reach = std::max(range, horizon) * BS + m_altitude * BS;
	m_showing = m_enabled && !range_all && range > m_cut;
	if (!m_showing)
		return;

	// the coarsest tiles that reach the range, split down toward the camera
	m_draw.clear();
	std::vector<std::pair<float, Key>> want;
	for (const TileKey &root : m_layout->roots(range))
		choose(root, m_draw, want);

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
		Entry &e = m_tiles[Key(k)];
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

void View::updateMask(ClientMap &map, v3f origin)
{
	// the map blocks the near world draws this frame, by column
	std::vector<std::vector<s16>> drawn((size_t)MASK * MASK);
	map.forEachDrawn([&](v3s16 block) {
		const std::optional<v2s32> at = m_layout->maskColumn(block, origin);
		if (at && at->X >= 0 && at->X < MASK && at->Y >= 0 && at->Y < MASK)
			drawn[(size_t)at->Y * MASK + at->X].push_back(block.Y);
	});
	auto *pixels = static_cast<u32 *>(m_mask->lock(video::ETLM_WRITE_ONLY, 0, 0, video::ETLF_NONE));
	if (!pixels)
		throw BaseException("far view: can't write its mask texture");
	m_near.assign((size_t)MASK * MASK, 0);
	for (s32 v = 0; v < MASK; v++)
	for (s32 u = 0; u < MASK; u++) {
		std::vector<s16> &ys = drawn[(size_t)v * MASK + u];
		bool near = !ys.empty(), top = near;
		// Where columns are whole (a cube planet's), the near world stands in
		// for one only if it draws all of its surface: at its edge it draws by
		// distance, and may have the seabed or a hill's foot without the top.
		// Where it draws the top but not the floor (low over the sea), its
		// water lies over our floor. A column whose surface we don't know
		// (only coarse tiles of ours cover it: high up, where the near world
		// draws only air) isn't the near world's.
		if (const std::optional<u32> column = m_layout->surfaceColumn(u, v)) {
			auto it = m_surfaces.find(*column);
			near = near && it != m_surfaces.end();
			top = near;
			if (near) {
				std::sort(ys.begin(), ys.end());
				s16 lo = getContainerPos(it->second.low, MAP_BLOCKSIZE), hi = getContainerPos(it->second.high, MAP_BLOCKSIZE);
				top = std::binary_search(ys.begin(), ys.end(), hi);
				for (s16 y = lo; y <= hi && near; y++)
					near = std::binary_search(ys.begin(), ys.end(), y);
			}
		}
		m_near[(size_t)v * MASK + u] = near;
		pixels[(size_t)v * MASK + u] = video::SColor(255, near ? 255 : 0, top ? 255 : 0, 0).color;
	}
	m_mask->unlock();
}

void View::evict()
{
	// surfaces of columns long out of range
	v2f eye(m_eye.X / MAP_BLOCKSIZE, m_eye.Z / MAP_BLOCKSIZE);
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

	v3f eye = camera->getAbsolutePosition();
	v3f origin = eye - m_eye * BS; // the scene position of map node (0, 0, 0)
	// the mask, from exactly the blocks the near world is about to draw
	// (its draw list is updated after our update(), and not every frame)
	updateMask(*m_map, origin);

	const v2f corner = m_layout->maskCorner(origin);
	m_state->cut = m_cut * BS;
	m_state->haze = std::max(m_max - m_cut, 1.0f) * BS;
	m_state->mask[0] = corner.X;
	m_state->mask[1] = corner.Y;
	m_state->mask[2] = (float)(MAP_BLOCKSIZE * MASK) * BS;
	m_state->mask[3] = 1;
	m_state->share = m_share;
	m_state->sea = origin.Y + m_horizon.water_level * BS;
	m_state->place[0] = m_eye.X;
	m_state->place[1] = m_eye.Z;
	m_state->place[2] = m_altitude;
	m_state->air = (float)terragen::sphere(m_layout->radius()).scale_height;

	// Share the camera's depth projection with near blocks and clouds. The
	// latter draw later and must be tested against the actual far terrain.
	driver->setMaterial(m_material);

	// every tile chosen is drawn: drawn curved, where its box is in the map
	// doesn't say whether it's in view
	for (const Key &key : m_draw) {
		auto it = m_tiles.find(key);
		if (it == m_tiles.end())
			continue;
		// the near world draws all of it: nothing of ours would show
		if (m_layout->covered(key.tile(), m_near))
			continue;
		const std::optional<core::matrix4> world = m_layout->transform(key.tile(), origin);
		if (!world)
			continue;
		driver->setTransform(video::ETS_WORLD, *world);
		for (const auto &part : it->second.parts)
			driver->drawMeshBuffer(part.get());
	}
}

} // namespace far
