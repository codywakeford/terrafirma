// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "far/palette.h"
#include "far/terrain.h"
#include "far/tiles.h"
#include "planet/atlas.h"
#include "planetarium/camera.h"
#include "planetarium/seams.h"
#include <memory>

struct SEvent;
namespace video { class IVideoDriver; }

/*
	The Planetarium (planning/sprint-31-planetarium.md): one planet viewer,
	native, drawn under whatever page is shown over it. A body's land is
	far::Tiles for the camera's eye, the same tiles the far view draws in
	game; layers draw over it (the atlas's seams, for now). Hosts make one
	and give it the window: the game (a key), and `luanti --planetarium`
	(a world code alone, from the game's baked palette).
*/
namespace planetarium {

class Planetarium
{
public:
	struct Opts
	{
		planet::Atlas atlas;
		far::Horizon horizon;
		far::Palette palette;
	};

	explicit Planetarium(Opts opts);

	// The mouse: drag to orbit, the wheel to go up and down. Whether it was
	// the Planetarium's.
	bool handle(const SEvent &event);

	// A frame, over the whole screen; the scene already begun. Throws if a
	// tile couldn't be built.
	void draw(video::IVideoDriver *driver);

private:
	const far::Palette m_palette; // before the tiles that read it
	far::Tiles m_tiles;
	OrbitCamera m_camera;
	SeamLayer m_seams;
	bool m_dragging = false;
	s32 m_x = 0, m_y = 0; // the mouse, while dragging
	u32 m_frame = 0;
};

} // namespace planetarium
