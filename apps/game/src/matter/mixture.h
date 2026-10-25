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
	double exchange = 50; // W/K with the surroundings (a crucible's; a cell of a sandbox exchanges with its neighbours instead: 0)
};

/*
	A mixture: amounts of substances (moles), with a temperature, at the
	pressure its surroundings hold it at. Its properties come from what's in
	it. Stepping it runs every reaction whose conditions are met, at its rate,
	and balances energy: reaction heat, latent heat as substances change
	state, and heat exchanged with the surroundings. The same primitive at
	every scale — a crucible, a room's air, an ocean, an atmosphere.

	Changes of state take their time: a substance crossing its melting or
	boiling point is split between the two states and the mixture stays at
	that temperature until the heat to finish (or undo) the change has come
	(ice in a warm room sits at 0 °C until it has melted). Matter added in
	another state than what's there resolves the same way (steam meeting cold
	water condenses, warming it).
*/
class Mixture
{
public:
	explicit Mixture(const Matter &table);

	std::vector<double> amounts; // moles, by substance index
	double temperature;          // kelvin
	double vessel = 1000;        // J/K: what holds it (a pot), so a few moles don't swing wildly

	// Each reaction's average rate over the run, in reactions per second.
	std::vector<double> run(double seconds, const Conditions &c);

	// The state most of a substance is in.
	State state(size_t substance, double pressure) const;
	// Moles of a substance in a state (two at once while it changes).
	double in(size_t substance, State state) const;
	// Add or take moles of a substance in a given state (moving matter
	// between mixtures). A mixture filled this way knows every state; one
	// filled through `amounts` learns them on its first run.
	void add(size_t substance, State state, double moles);
	void take(size_t substance, State state, double moles);
	// Joules per kelvin: everything in it, plus the vessel.
	double heatCapacity() const;

private:
	// How far a reaction could go on what's here (its limiting reactant).
	double room(const Reaction &r) const;
	bool met(const Reaction &r, double pressure) const;
	double rate(const Reaction &r, const Conditions &c) const;

	// Each substance's state; while one changes, `m_lagging` moles of it are
	// still in `m_from` (the rest in `m_states`).
	void track(double pressure);
	void settle(double pressure);

	const Matter &m_table;
	std::vector<State> m_states, m_from;
	std::vector<double> m_lagging;
};

} // namespace matter
