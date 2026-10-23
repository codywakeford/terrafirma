// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "fixed.h"
#include <stdexcept>
#include <string>

namespace terragen {

// A world code that can't be read. The message is meant for the player.
struct CodeError : std::runtime_error
{
	using std::runtime_error::runtime_error;
};

struct Star
{
	const char *id;
	fix warmth; // shifts the whole planet's climate (Earth's terrain model)
	fix mass;   // solar masses; luminosity follows as mass⁴ (the body model)
};

// Coolest to hottest; the index is what a world code stores. Must match
// ui/src/worldgen/config.ts STARS.
extern const Star STARS[5];

/*
	A decoded world code: TF5.<seed>.<params>, as ui/src/worldgen/config.ts
	writes it. (TF1: sprint 1, without water or exact land; TF2: sprint 2, before
	plates and geology; TF3: sprint 9, before the climate; TF4: sprint 13, before the ocean currents.)
	Parameters are kept as the fixed values their quantized bytes stand for,
	so the same code always gives exactly the same numbers.
*/
struct WorldCode
{
	std::string text;      // the code itself, exactly as given
	std::string seed;      // as typed (UTF-8)
	uint32_t seed_hash;    // FNV-1a of the seed's bytes: terragen's randomness
	int32_t size;          // blocks per side
	const Star *star;
	int32_t continents;    // 1..8
	fix land, coast, mountains, roughness; // 0..1
	fix temperature, humidity;             // −1..1
	fix rivers, caves, ores;               // 0..1
	fix tilt;              // degrees, 0..45 (whole degrees)
	int32_t year;          // days, 8..128

	static WorldCode decode(const std::string &code); // throws CodeError
};

} // namespace terragen
