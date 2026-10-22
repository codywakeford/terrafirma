// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "pixels.h"

namespace shell {

void Pixels::write(const void *premultiplied, u32 w, u32 h)
{
	width = w;
	height = h;
	bgra.resize((size_t)w * h * 4);

	const u8 *src = static_cast<const u8 *>(premultiplied);
	u8 *dst = bgra.data();
	for (size_t i = 0; i < bgra.size(); i += 4) {
		u8 a = src[i + 3];
		if (a == 0 || a == 255) {
			dst[i] = src[i];
			dst[i + 1] = src[i + 1];
			dst[i + 2] = src[i + 2];
		} else {
			dst[i] = (u8)(src[i] * 255 / a);
			dst[i + 1] = (u8)(src[i + 1] * 255 / a);
			dst[i + 2] = (u8)(src[i + 2] * 255 / a);
		}
		dst[i + 3] = a;
	}
	dirty = true;
}

u8 Pixels::alphaAt(s32 x, s32 y) const
{
	if (x < 0 || y < 0 || (u32)x >= width || (u32)y >= height)
		return 0;
	return bgra[((size_t)y * width + x) * 4 + 3];
}

} // namespace shell
