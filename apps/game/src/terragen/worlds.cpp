// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "worlds.h"
#include "system.h"
#include <cmath>
#include <stdexcept>

namespace terragen {

namespace {

// A body's land by its kind: the simplest that reads as what it is (its own
// terrain, craters and dunes, is the terrain work's)
Surface surfaceOf(Kind kind)
{
	switch (kind) {
	case Kind::Temperate: return Surface::Living;
	case Kind::ColdDesert:
	case Kind::Hothouse:
	case Kind::Hydrocarbon: return Surface::Sand;
	case Kind::Volcanic:
	case Kind::Airless: return Surface::Rock;
	case Kind::IceShell: return Surface::Ice;
	case Kind::Giant:
	case Kind::Count: break;
	}
	throw std::invalid_argument("terragen::bodyWorlds: no surface for a body of this kind");
}

} // namespace

std::vector<BodyWorld> bodyWorlds(const WorldCode &home, const Atlas::Params &homeAtlas)
{
	const System system(SystemSpec::forWorld(home));
	const size_t playable = system.playable();
	if (system.cards()[playable].giant)
		throw std::invalid_argument("terragen::bodyWorlds: the home is a giant, with no surface");
	const double blocksPerKm = homeAtlas.radius / system.radius(playable);
	std::vector<BodyWorld> out{ { playable, system.spec().bodies[playable].id, home, homeAtlas } };
	for (size_t b = 0; b < system.spec().bodies.size(); b++) {
		const double radius = system.radius(b) * blocksPerKm;
		if (b == playable || system.cards()[b].giant || radius < SMALLEST_BODY)
			continue;
		const std::string &id = system.spec().bodies[b].id;
		WorldCode code = home;
		code.surface = surfaceOf(system.cards()[b].kind);
		code.text = home.text + "@" + id; // what the world model's grid is kept by
		code.seed = home.seed + "@" + id;
		code.seed_hash = home.seed_hash * 16777619u ^ (uint32_t)std::hash<std::string>{}(id);
		code.size = (int32_t)std::lround(2 * M_PI * radius);
		out.push_back({ b, id, code, Atlas::standard(radius) });
	}
	return out;
}

} // namespace terragen
