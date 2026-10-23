// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "code.h"
#include "fixed.h"
#include <vector>

/*
	The body model: a world is a short history, not a list of settings. A
	few causes — the star, the orbit, the body's mass, where it formed, what
	it formed with, how it spins, whether life arose — are stepped through
	time from formation to the system's age; what the body is today emerges
	(its air, its temperature, its field, its seas, its kind), along with the
	story of how it got there. Units are relative to Earth (= 1) unless
	noted. Fixed point and pure, like the rest of terragen: the menu's
	preview and the game read the same numbers. See
	planning/systems-and-bodies.md.
*/
namespace terragen {

struct StarCauses
{
	int32_t type;  // index into STARS
	fix age;       // the system's age, billions of years
	fix activity;  // 0..1: how hard its wind blows
};

struct BodyCauses
{
	int32_t parent;   // −1 orbits the star; else the index of the planet it orbits (listed before it)
	fix orbit;        // AU around the star; parent radii around a planet
	fix eccentricity; // 0..0.9
	fix mass;         // Earth masses
	fix formed;       // 0..1: where it formed, hot and close (0) to beyond the frost line (1)
	fix volatiles;    // 0..1: the water and gases it formed with
	fix day;          // rotation period, hours
	bool life;        // whether life arises where it can
};

enum class Kind : uint8_t {
	Temperate, ColdDesert, Hothouse, Volcanic, IceShell, Hydrocarbon, Airless, Giant,
	Count
};
extern const char *const KIND_NAMES[(int)Kind::Count];

enum class Gas : uint8_t { CO2, N2, O2, CH4, H2, He, Count };
extern const char *const GAS_NAMES[(int)Gas::Count];

// What can happen in a body's history.
enum class Event : uint8_t {
	FieldOn, FieldFades, OceansForm, OceansBoil, OceansFreeze, SeasComeAndGo,
	PlatesBegin, PlatesStop, VolcanoesSilent, AirLost, Runaway, LifeAppears, OxygenRises,
	Count
};
extern const char *const EVENT_TEXT[(int)Event::Count];

struct Moment
{
	fix age;   // billions of years after formation
	Event event;
};

// One shell of a body, from the centre out.
struct Layer
{
	const char *name; // "core", "mantle", "crust", "ocean", "ice", "metallic hydrogen", "envelope"
	fix outer;        // its outer edge, as a share of the radius
	bool molten;      // liquid (a core that runs a field, an ocean, an envelope)
};

struct BodyCard
{
	Kind kind;
	bool giant;
	fix metal, rock, ice;   // composition shares
	fix radius, gravity;    // Earth = 1
	fix escape;             // km/s
	fix sunlight;           // starlight received, Earth = 1
	fix temperature;        // surface (cloud tops for a giant), kelvin
	fix equilibrium;        // without an atmosphere, kelvin
	fix pressure;           // bar at the surface; −1 for a giant (no surface)
	fix gases[(int)Gas::Count]; // shares of the air, summing to 1 (all 0 when there's none)
	fix field;              // magnetic field strength
	fix heat;               // interior heat flow, tides included
	fix tidal;              // the tidal part of it
	bool ocean, plates, volcanism, locked;
	std::vector<Layer> layers; // centre outward
	std::vector<Moment> history;
};

// Every body's card, in the order given. Throws std::invalid_argument on a
// system that can't exist (a moon of a moon, a parent listed after its moon,
// out-of-range causes).
std::vector<BodyCard> bodies(const StarCauses &star, const std::vector<BodyCauses> &causes);

} // namespace terragen
