// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "planet.h"

namespace terragen {

Planet home()
{
	// the air: sea-level density, and the scale height of the lower air
	return { EARTH_RADIUS / 10, 9.81, 86400.0 / 10, 1361, 1.225, 7200 };
}

Planet resized(const Planet &p, double radius)
{
	Planet r = p;
	r.radius = radius;
	r.day = p.day * (radius / p.radius);
	return r;
}

} // namespace terragen
