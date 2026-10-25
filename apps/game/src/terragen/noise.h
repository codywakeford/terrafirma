// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "fixed.h"

namespace terragen {

/*
	Value noise over the world square (u, v) ∈ [0,1)², periodic in u: lattice
	columns wrap modulo the frequency, so the field joins seamlessly east–west
	with no trigonometry. `frequency` is lattice cells across one world width;
	each octave doubles it, so every octave stays periodic. Results in −1..1.
*/
fix value(fix u, fix v, uint32_t seed, int32_t frequency);
fix fbm(fix u, fix v, uint32_t seed, int32_t frequency, int octaves, fix persistence);

/*
	Value noise in 3D block coordinates, for what varies with depth (rock
	folds, ore veins, caves). `cell` is the lattice spacing in blocks; x
	wraps every `period` blocks (the world's width), so it joins east–west
	like the 2D noise. x is a block coordinate in [0, period). Results in −1..1.
*/
fix value3(int32_t x, int32_t y, int32_t z, uint32_t seed, int32_t cell, int32_t period);

} // namespace terragen
