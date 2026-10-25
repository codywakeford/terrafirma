// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "irrlichttypes.h"
#include <include/internal/cef_types_wrappers.h>
#include <vector>

namespace shell {

// The page's last frame on the CPU, straight (not premultiplied) BGRA.
struct Pixels
{
	u32 width = 0;
	u32 height = 0;
	std::vector<u8> bgra;
	bool dirty = false;

	// Chromium paints premultiplied BGRA; Irrlicht's 2D drawing blends straight
	// alpha. Only the rectangles Chromium reports as changed are converted.
	void write(const void *premultiplied, u32 w, u32 h, const std::vector<CefRect> &changed);
	u8 alphaAt(s32 x, s32 y) const;
};

} // namespace shell
