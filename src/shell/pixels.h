// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "irrlichttypes.h"
#include <vector>

namespace shell {

// The page's last frame on the CPU, straight (not premultiplied) BGRA.
struct Pixels
{
	u32 width = 0;
	u32 height = 0;
	std::vector<u8> bgra;
	bool dirty = false;

	// Chromium paints premultiplied BGRA; Irrlicht blends straight alpha.
	void write(const void *premultiplied, u32 w, u32 h);
	u8 alphaAt(s32 x, s32 y) const;
};

} // namespace shell
