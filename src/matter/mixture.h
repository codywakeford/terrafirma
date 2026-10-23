// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "matter.h"
#include <vector>

namespace matter {

// What a mixture's surroundings give it.
struct Conditions
{
	double surroundings; // kelvin: what it exchanges heat with (a furnace, the air)
	double pressure;     // pascals
	double power;        // watts of electric current through it
	double light;        // watts of light on it
};

/*
	A mixture: amounts of substances (moles), with a temperature, at the
	pressure its surroundings hold it at. Its properties come from what's in
	it. Stepping it runs every reaction whose conditions are met, at its rate,
	and balances energy: reaction heat, latent heat as substances change
	state, and heat exchanged with the surroundings. The same primitive at
	every scale — a crucible, a room's air, an ocean, an atmosphere.
*/
class Mixture
{
public:
	explicit Mixture(const Matter &table);

	std::vector<double> amounts; // moles, by substance index
	double temperature;          // kelvin

	// Each reaction's average rate over the run, in reactions per second.
	std::vector<double> run(double seconds, const Conditions &c);

	State state(size_t substance, double pressure) const;
	// Joules per kelvin: everything in it, plus the vessel.
	double heatCapacity() const;

private:
	// How far a reaction could go on what's here (its limiting reactant).
	double room(const Reaction &r) const;
	bool met(const Reaction &r, double pressure) const;
	double rate(const Reaction &r, const Conditions &c) const;

	const Matter &m_table;
	std::vector<State> m_states; // last step's, to charge latent heat on changes
};

} // namespace matter
