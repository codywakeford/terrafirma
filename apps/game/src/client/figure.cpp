// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "figure.h"

#include "client/mesh.h"
#include "constants.h"
#include <IAnimatedMesh.h>
#include <IMeshSceneNode.h>
#include <ISceneManager.h>
#include <algorithm>
#include <cmath>

namespace {

// Its proportions (nodes) before it's scaled to the player's height: legs
// and torso three quarters of a node each, a head of half.
constexpr float TALL = 2.0f, HEIGHT = 1.77f, S = HEIGHT / TALL;
// A stride: a leg's swing back and forth per this far walked (nodes).
constexpr float STRIDE = 1.6f;
constexpr float WALK = 4, RUN = 6.4f; // nodes/s

const video::SColor SKIN(255, 216, 160, 122), SHIRT(255, 0, 148, 210), TROUSERS(255, 58, 63, 85);

// Toward `to` at most `rate` × dtime.
float approach(float from, float to, float rate, float dtime)
{
	const float most = rate * dtime;
	return from + std::clamp(to - from, -most, most);
}

} // namespace

Figure::Figure(scene::ISceneManager *smgr, scene::ISceneNode *parent, video::ITexture *white,
		const std::function<void(scene::ISceneNode *)> &material) :
	m_smgr(smgr), m_white(white), m_material(material)
{
	m_root = smgr->addEmptySceneNode(parent);
	m_root->grab();
	// the torso turns at the hips; the head and arms go with it
	m_torso = limb(m_root, v3f(0, 0.75f, 0), v3f(0.5f, 0.75f, 0.25f), v3f(0, 0.375f, 0), SHIRT);
	m_head = limb(m_torso.pivot, v3f(0, 0.75f, 0), v3f(0.5f), v3f(0, 0.25f, 0), SKIN);
	for (int side : { 0, 1 }) {
		const float x = side ? -1.f : 1.f;
		m_arms[side] = limb(m_torso.pivot, v3f(0.375f * x, 0.625f, 0), v3f(0.25f, 0.75f, 0.25f), v3f(0, -0.25f, 0), SHIRT);
		m_legs[side] = limb(m_root, v3f(0.125f * x, 0.75f, 0), v3f(0.25f, 0.75f, 0.25f), v3f(0, -0.375f, 0), TROUSERS);
	}
}

Figure::~Figure()
{
	m_root->remove(); // its limbs with it
	m_root->drop();
}

Figure::Limb Figure::limb(scene::ISceneNode *parent, v3f joint, v3f size, v3f centre, video::SColor colour)
{
	scene::ISceneNode *pivot = m_smgr->addEmptySceneNode(parent);
	pivot->setPosition(joint * S * BS);
	scene::IAnimatedMesh *mesh = createCubeMesh(size * S * BS);
	setMeshColor(mesh, colour);
	scene::IMeshSceneNode *box = m_smgr->addMeshSceneNode(mesh, pivot);
	mesh->drop();
	box->setPosition(centre * S * BS);
	m_material(box);
	box->forEachMaterial([this](video::SMaterial &mat) { mat.setTexture(0, m_white); });
	return { pivot, box };
}

void Figure::pose(float dtime, v3f velocity, float pitch, bool airborne)
{
	if (dtime <= 0)
		return;
	const float across = std::hypot(velocity.X, velocity.Z);
	// the stride follows the ground covered, backwards when walking back
	const float ahead = velocity.Z >= -0.1f * across ? 1.f : -1.f;
	m_phase = std::fmod(m_phase + ahead * across * dtime / STRIDE * 2 * core::PI, 2 * core::PI);
	m_swing = approach(m_swing, std::min(across / WALK, RUN / WALK) * 0.55f, 4, dtime);
	m_lean = approach(m_lean, 0.25f * std::clamp((across - WALK) / (RUN - WALK), 0.f, 1.f), 1.5f, dtime);
	m_air = approach(m_air, airborne ? 1.f : 0.f, 6, dtime);

	const float swing = std::sin(m_phase) * m_swing * (1 - m_air);
	const float deg = core::RADTODEG;
	m_torso.pivot->setRotation(v3f(m_lean * deg, 0, 0));
	m_head.pivot->setRotation(v3f((std::clamp(pitch, -1.2f, 1.2f) - m_lean) * deg, 0, 0));
	for (int side : { 0, 1 }) {
		const float way = side ? -1.f : 1.f;
		// legs opposite each other; each arm opposite its leg; in the air,
		// one leg forward and one back, arms out
		m_legs[side].pivot->setRotation(v3f((swing * way + m_air * 0.4f * way) * deg, 0, 0));
		m_arms[side].pivot->setRotation(v3f((-swing * way * 0.9f - m_lean) * deg, 0, m_air * 0.5f * way * deg));
	}
}

void Figure::light(video::SColor colour)
{
	for (scene::IMeshSceneNode *box : { m_torso.box, m_head.box, m_arms[0].box, m_arms[1].box, m_legs[0].box, m_legs[1].box })
		box->forEachMaterial([&](video::SMaterial &mat) { mat.ColorParam = colour; });
}
