// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "irrlichttypes.h"

namespace video { class IVideoDriver; class ITexture; }

namespace shell {

struct Pixels;

// Owns the GPU texture the page is drawn from, and draws it over the frame.
class Surface
{
public:
	explicit Surface(video::IVideoDriver *driver);
	~Surface();

	void draw(Pixels &pixels);

private:
	void upload(Pixels &pixels);

	video::IVideoDriver *m_driver;
	video::ITexture *m_texture = nullptr;
};

} // namespace shell
