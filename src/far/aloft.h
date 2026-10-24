// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

namespace far {

// Where a player is over the planet when that isn't where they stand: in
// space, a bubble's cell is somewhere else on the map, and the far view
// draws the planet from here instead (TOCLIENT_ALOFT).
struct Aloft
{
	float x, z;              // map blocks, fractional
	float altitude;          // m above the sea
	float east, north, climb; // their rates: blocks/s, blocks/s, m/s

	// Carried forward `seconds` (the server says it a few times a second).
	Aloft after(float seconds) const
	{
		return { x + east * seconds, z + north * seconds, altitude + climb * seconds, east, north, climb };
	}
};

} // namespace far
