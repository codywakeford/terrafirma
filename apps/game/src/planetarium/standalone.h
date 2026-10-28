// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <string>

namespace planetarium {

/*
	`luanti --planetarium [code]`: the Planetarium alone in a window, on the
	planet a world code makes (the default world's if none), its land from
	the game's baked palette (far/baked.h): no world, no server, nothing
	saved. For looking at what the generator makes. Escape closes it.
	Throws if the code doesn't decode or the palette won't load.
*/
void runStandalone(const std::string &code);

} // namespace planetarium
