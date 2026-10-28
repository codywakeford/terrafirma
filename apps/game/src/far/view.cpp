// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "view.h"
#include "far/mesh.h"
#include "far/tiles.h"
#include "client/atlas_view.h"
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
#include <ISceneManager.h>
#include <IVideoDriver.h>
#include <algorithm>
#include <cmath>

namespace far {

// A tile splits while it's nearer than this many of its own spans past the
// near world's edge, by the Horizon detail setting (1..4).
static const float SPLIT[4] = { 1.0f, 1.5f, 2.0f, 3.0f };
// The air's depth, in its scale heights: above it the haze has given way
// to the planet seen from above.
static constexpr double AIR_DEPTH = 8;
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

static void settingChanged(const std::string &, void *data)
{
	static_cast<View *>(data)->readSettings();
}

static const char *const SETTINGS[] = { "far_view", "far_view_detail", "far_view_max" };

View::View(scene::ISceneManager *smgr, IShaderSource *ssrc, ITextureSource *tsrc,
		const NodeDefManager *ndef, const Horizon &horizon, const planet::Atlas &atlas) :
	scene::ISceneNode(smgr->getRootSceneNode(), smgr, -1),
	m_water_level(horizon.water_level),
	m_radius(atlas.atlas().params().radius),
	m_colours(std::make_unique<Colours>(ndef, tsrc, blocksIn(horizon))),
	m_tiles(std::make_unique<Tiles>(atlas, horizon, *m_colours)),
	m_state(std::make_shared<DrawState>())
{
	m_uniforms = make_irr<Uniforms>(m_state);
	u32 shader = ssrc->getShader("far_globe_shader", ShaderConstants{}, video::EMT_SOLID, m_uniforms.get());
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
	m_tiles.reset(); // before the colours its workers read
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

void View::update(const Camera &camera, ClientMap &map, float near_range, bool range_all,
		std::optional<float> fog_limit)
{
	const v3f eye = (camera.getCameraNode()->getPosition() + intToFloat(camera.getOffset(), BS)) / BS;
	m_view = map.atlasView();
	const std::optional<planet::Atlas::Frame> frame = m_view ? m_view->eye() : std::nullopt;
	m_showing = false;
	if (!frame)
		return; // the eye has no grid this frame (seam): nothing is drawn from it

	// the eye over the sea, and how high it is
	const terragen::Atlas::Vec &o = frame->origin;
	const double centre = std::sqrt(o.x * o.x + o.y * o.y + o.z * o.z);
	const double r = m_radius, sea = r + m_water_level + 1;
	m_altitude = (float)std::max(centre - sea, 0.0);
	const double h = m_altitude;
	// up through the air the haze gives way to the planet seen from above
	m_share = (float)std::clamp(h / (AIR_DEPTH * terragen::sphere(r).scale_height), 0.0, 1.0);

	// out to the planet's horizon (at most half way round), along the planet
	const float horizon = (float)std::sqrt(2 * r * h + h * h); // m
	float range = std::min(m_max, (float)(M_PI * r));
	if (fog_limit)
		range = std::min(range, *fog_limit);
	else
		range = std::min(std::max(range, 1.1f * horizon), (float)(M_PI * r));
	m_cut = near_range;
	m_reach = std::max(range, horizon) * BS + m_altitude * BS;
	m_showing = m_enabled && !range_all && range > m_cut;
	if (!m_showing)
		return;

	// a block is a metre: the height counts toward a tile's distance as is
	const Eye at{ { o.x / centre * sea, o.y / centre * sea, o.z / centre * sea }, m_altitude, m_cut, range, m_split };
	m_tiles->update(at);
	m_map = &map;
	m_eye = eye;
}

void View::updateMask(ClientMap &map, v2f corner)
{
	// the map-block-wide columns round the eye the near world draws a block in, this frame
	std::vector<u8> near((size_t)MASK * MASK, 0);
	const float w = MAP_BLOCKSIZE * BS;
	map.forEachDrawn([&](v3s16 block) {
		const core::matrix4 *m = m_view->transform(block);
		if (!m)
			return;
		const v3f at = m->getTranslation(); // the block's middle, where it's drawn
		const s32 u = (s32)std::floor((at.X - corner.X) / w), v = (s32)std::floor((at.Z - corner.Y) / w);
		if (u >= 0 && u < MASK && v >= 0 && v < MASK)
			near[(size_t)v * MASK + u] = 1;
	});
	auto *pixels = static_cast<u32 *>(m_mask->lock(video::ETLM_WRITE_ONLY, 0, 0, video::ETLF_NONE));
	if (!pixels)
		throw BaseException("far view: can't write its mask texture");
	for (size_t i = 0; i < near.size(); i++)
		pixels[i] = video::SColor(255, near[i] ? 255 : 0, near[i] ? 255 : 0, 0).color;
	m_mask->unlock();
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

	const v3f origin = camera->getAbsolutePosition() - m_eye * BS; // the scene position of map node (0, 0, 0)
	const float half = MASK / 2 * MAP_BLOCKSIZE * BS;
	const v2f corner(origin.X + m_eye.X * BS - half, origin.Z + m_eye.Z * BS - half);
	// the mask, from exactly the blocks the near world is about to draw
	// (its draw list is updated after our update(), and not every frame)
	updateMask(*m_map, corner);

	m_state->cut = m_cut * BS;
	m_state->haze = std::max(m_max - m_cut, 1.0f) * BS;
	m_state->mask[0] = corner.X;
	m_state->mask[1] = corner.Y;
	m_state->mask[2] = (float)(MAP_BLOCKSIZE * MASK) * BS;
	m_state->mask[3] = 1;
	m_state->share = m_share;
	m_state->sea = origin.Y + m_water_level * BS;
	m_state->place[0] = m_eye.X;
	m_state->place[1] = m_eye.Z;
	m_state->place[2] = m_altitude;
	m_state->air = (float)terragen::sphere(m_radius).scale_height;

	// Share the camera's depth projection with near blocks and clouds. The
	// latter draw later and must be tested against the actual far terrain.
	driver->setMaterial(m_material);
	for (const Tiles::Drawn &tile : m_tiles->drawn()) {
		const std::optional<core::matrix4> world = m_view->place(tile.middle);
		if (!world)
			continue;
		driver->setTransform(video::ETS_WORLD, *world);
		for (const auto &part : tile.parts)
			driver->drawMeshBuffer(part.get());
	}
}

} // namespace far
