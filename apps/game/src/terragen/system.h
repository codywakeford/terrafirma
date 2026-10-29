// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "body.h"
#include "code.h"
#include <string>
#include <vector>

/*
	A planetary system: what a new world is (planning/sprint-35-system.md).
	The star and every body round it as the player made it (causes only),
	what each body is today (its card, from the body model: body.h), and
	where each is on its orbit at a moment. The menu, the create screen and
	the game all read this one model; the page edits causes, never computes
	a body.

	Orbits are Kepler's, round the star for a planet and round its parent
	for a moon, their periods from the masses, in real units (km, days).
	They're doubles: a position places a body in a view, it never decides a
	block (the causes and cards, which can, are fixed point).
*/
namespace terragen {

struct SystemBody
{
	std::string id, name;
	std::string colour; // "#rrggbb": its swatch in lists (the page's, as it was designed)
	BodyCauses causes;
	double phase;       // 0..1: where on its orbit it is at day 0
	double inclination; // degrees: its orbit's tilt from its parent's plane (the star's: the ecliptic)
	double tilt;        // degrees: its spin axis's from its orbit's
	bool playable;      // the body the world is played on (one per system)
};

struct SystemSpec
{
	StarCauses star;
	std::vector<SystemBody> bodies; // a planet before its moons

	// The starter system: like ours, so the model's worlds are
	// recognisable. A Mercury, an Earth and Moon, a small Mars with a moon,
	// a Jupiter with four.
	static SystemSpec home();
	// A world's system: the home system round the world code's star, its
	// playable body tilted as the code says. (Until a code records a whole
	// system: sprint 35.)
	static SystemSpec forWorld(const WorldCode &code);
};

class System
{
public:
	struct Vec
	{
		double x, y, z;
	};

	// Throws std::invalid_argument for a system that can't exist (the body
	// model's checks; none or several playable bodies).
	explicit System(SystemSpec spec);

	const SystemSpec &spec() const { return m_spec; }
	const std::vector<BodyCard> &cards() const { return m_cards; }
	size_t playable() const { return m_playable; }

	// Where a body is `days` after day 0, km from the star: the ecliptic is
	// x–z, y its north.
	Vec position(size_t body, double days) const;
	double radius(size_t body) const; // km (a giant's to its cloud tops)
	double period(size_t body) const; // days, round what it orbits
	double starRadius() const;        // km
	double luminosity() const;        // the Sun's = 1
	// The system's day for a world's: as far through the playable body's
	// orbit as the world is through its year (a world's year is its own
	// length, shorter than its orbit's).
	double dayFor(double worldDays, int32_t worldYear) const;

private:
	// where a body is round what it orbits, km
	Vec local(size_t body, double days) const;

	SystemSpec m_spec;
	std::vector<BodyCard> m_cards;
	size_t m_playable = 0;
};

} // namespace terragen
