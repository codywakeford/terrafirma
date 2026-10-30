// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "irr_v3d.h"
#include <ISceneNode.h>
#include <SColor.h>
#include <functional>

namespace scene { class IMeshSceneNode; class ISceneManager; }
namespace video { class ITexture; }

/*
	A player's figure (sprint 58, visual "body"): a jointed body of boxes in
	a person's proportions (Minecraft-ish: head, torso, two arms, two legs),
	each limb turning at its joint. Posed every frame from how the player
	moves: legs and arms swing with the distance walked (so feet keep pace
	with the ground), wider at a run; the body leans into a run; the arms
	go out and the legs bend in the air; the head looks where the player
	looks. No animation clips: movement is the animation.
*/
class Figure
{
public:
	// Under `parent` (the entity's frame: +z forward, feet at 0), its boxes
	// textured `white` and coloured, each made a material by `material`.
	Figure(scene::ISceneManager *smgr, scene::ISceneNode *parent, video::ITexture *white,
			const std::function<void(scene::ISceneNode *)> &material);
	~Figure();
	Figure(const Figure &) = delete;
	Figure &operator=(const Figure &) = delete;

	// What the entity's drawn as: hiding it hides the figure.
	scene::ISceneNode *root() const { return m_root; }

	// Posed after `dtime` moving at `velocity` (nodes/s, the figure's own
	// frame: +z ahead), looking `pitch` (radians, + down), in the air or not.
	void pose(float dtime, v3f velocity, float pitch, bool airborne);
	// Lit as its place is (the entity's light).
	void light(video::SColor colour);

private:
	struct Limb
	{
		scene::ISceneNode *pivot;   // turns at the joint
		scene::IMeshSceneNode *box;
	};
	Limb limb(scene::ISceneNode *parent, v3f joint, v3f size, v3f centre, video::SColor colour);

	scene::ISceneManager *m_smgr;
	video::ITexture *m_white;
	std::function<void(scene::ISceneNode *)> m_material;
	scene::ISceneNode *m_root;
	Limb m_torso, m_head, m_arms[2], m_legs[2];
	float m_phase = 0;  // radians through a stride
	float m_swing = 0;  // how far the limbs swing now (radians)
	float m_lean = 0;   // into a run (radians)
	float m_air = 0;    // 0 on the ground .. 1 in the air
};
