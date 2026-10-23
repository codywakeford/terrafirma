// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "skyparams.h"

/*
	The real sun in the client's sky. Visual only, so floats: the same
	formulas as terragen/sun.cpp, which decides gameplay. time_of_day: 0..1,
	0.5 = noon.
*/
namespace solar {

// The sun's height above the horizon, degrees.
float elevation(const SkySolar &s, float time_of_day);

// The stock time of day at which the stock sky's sun stands as high as the
// real sun does now, on the same side of noon. Everything that colours and
// lights the world is fed this, so the stock look follows the real sun: polar
// night stays dark, the midnight sun stays light.
float apparentTime(const SkySolar &s, float time_of_day);

} // namespace solar
