// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "irrlichttypes_bloated.h"
#include "planetarium/system.h"
#include <ISceneNode.h>
#include <optional>

class Camera;
class ClientMap;
namespace far { struct Horizon; }
namespace planet { class Atlas; }

/*
	The sky (planning/sprint-60-sky.md): the Planetarium's system seen from
	where the player is. The stock sky is off on a planet; behind the far
	view (which draws the planet itself) is space: the field of stars, the
	star and the other bodies where they are on the world's date, turned as
	the planet has turned by its time of day. No air yet: the sky is black
	by day too, till the atmosphere is drawn.

	Drawn first in the sky pass (made before the far view), its own depth
	ranges, then the depth cleared and the camera's own put back for what
	follows.
*/
namespace planetarium {

class Sky : public scene::ISceneNode
{
public:
	Sky(scene::ISceneManager *smgr, const planet::Atlas &atlas, const far::Horizon &horizon, int body);

	// Once a frame, before drawing: the eye, and the world's date (days
	// since its day 0, with the time of day as the fraction).
	void update(const Camera &camera, ClientMap &map, double worldDays);

	void OnRegisterSceneNode() override;
	void render() override;
	const aabb3f &getBoundingBox() const override { return m_box; }

private:
	SystemLayer m_system;
	const int32_t m_year; // the world's year, in its days
	aabb3f m_box{ -1e7f, -1e7f, -1e7f, 1e7f, 1e7f, 1e7f };

	// this frame's, in space (the system's frame round the body)
	struct Eye
	{
		terragen::System::Vec at; // from the body's centre, blocks
		core::matrix4 view;       // camera-relative
		float fov;                // radians, vertical
		double day;               // the system's
	};
	std::optional<Eye> m_eye; // none: the eye has no grid (seam)
};

} // namespace planetarium
