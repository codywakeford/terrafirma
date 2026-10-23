// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "mixture.h"
#include <algorithm>
#include <cmath>

namespace matter {

static constexpr double VESSEL = 1000.0;       // J/K: the pot itself, so a few moles don't swing wildly
static constexpr double EXCHANGE = 50.0;       // W/K with the surroundings
static constexpr double LONGEST_STEP = 10.0;   // seconds
static constexpr int MOST_STEPS = 400000;      // a run that needs more is stiff beyond reason

Mixture::Mixture(const Matter &table) :
	amounts(table.substances().size(), 0.0), temperature(293.15), m_table(table)
{
}

State Mixture::state(size_t i, double pressure) const
{
	return m_table.substances()[i].state(temperature, pressure);
}

double Mixture::heatCapacity() const
{
	double c = VESSEL;
	const auto &subs = m_table.substances();
	for (size_t i = 0; i < amounts.size(); i++)
		c += amounts[i] * subs[i].molar_mass / 1000.0 * subs[i].heat_capacity;
	return c;
}

double Mixture::room(const Reaction &r) const
{
	double most = INFINITY;
	for (const Reaction::Term &t : r.reactants)
		most = std::min(most, amounts[t.substance] / t.count);
	return most;
}

bool Mixture::met(const Reaction &r, double pressure) const
{
	for (const Reaction::Requirement &q : r.needs)
		if (amounts[q.substance] <= 0 || state(q.substance, pressure) != q.state)
			return false;
	return true;
}

double Mixture::rate(const Reaction &r, const Conditions &c) const
{
	double limit = room(r);
	if (limit <= 1e-12 || !met(r, c.pressure))
		return 0;
	switch (r.energy) {
	case Energy::None:
		return r.factor * std::exp(-r.activation * 1000.0 / (GAS_CONSTANT * temperature)) * limit;
	case Energy::Electricity:
		return c.power / (-r.heat * 1000.0);
	case Energy::Light:
		return c.light / (-r.heat * 1000.0);
	}
	return 0;
}

static double latent(const Substance &s, State from, State to)
{
	// energy to go from one state to another, per mole (J); negative gives heat back
	auto level = [&](State st) {
		switch (st) {
		case State::Solid: return 0.0;
		case State::Liquid: return s.fusion * 1000.0;
		case State::Gas:
		case State::Supercritical: return (s.fusion + s.vaporization) * 1000.0;
		}
		return 0.0;
	};
	return level(to) - level(from);
}

std::vector<double> Mixture::run(double seconds, const Conditions &c)
{
	const auto &reactions = m_table.reactions();
	const auto &subs = m_table.substances();
	std::vector<double> done(reactions.size(), 0.0);
	if (m_states.size() != amounts.size()) {
		m_states.clear();
		for (size_t i = 0; i < amounts.size(); i++)
			m_states.push_back(state(i, c.pressure));
	}

	double left = seconds;
	int steps = 0;
	std::vector<double> rates(reactions.size());
	while (left > 1e-9) {
		if (++steps > MOST_STEPS)
			throw MatterError("a mixture needed more than " + std::to_string(MOST_STEPS) + " steps for " +
					std::to_string(seconds) + " s");
		// how fast everything goes now, and how long a step that keeps it honest is
		double dt = std::min(left, LONGEST_STEP), heat = EXCHANGE * (c.surroundings - temperature);
		for (size_t j = 0; j < reactions.size(); j++) {
			rates[j] = rate(reactions[j], c);
			if (rates[j] > 0) {
				dt = std::min(dt, 0.1 * room(reactions[j]) / rates[j] + 1e-6);
				if (reactions[j].energy == Energy::None)
					heat += rates[j] * reactions[j].heat * 1000.0;
			}
		}
		double capacity = heatCapacity();
		if (heat != 0)
			dt = std::min(dt, 20.0 / (std::fabs(heat) / capacity));
		dt = std::max(dt, 1e-6);

		// react, never past what's there
		double released = EXCHANGE * (c.surroundings - temperature) * dt;
		for (size_t j = 0; j < reactions.size(); j++) {
			if (rates[j] <= 0)
				continue;
			const Reaction &r = reactions[j];
			double extent = std::min(rates[j] * dt, room(r));
			for (const Reaction::Term &t : r.reactants)
				amounts[t.substance] = std::max(0.0, amounts[t.substance] - t.count * extent);
			for (const Reaction::Term &t : r.products)
				amounts[t.substance] += t.count * extent;
			if (r.energy == Energy::None) // driven reactions take their heat from the power, not the pot
				released += extent * r.heat * 1000.0;
			done[j] += extent;
		}
		temperature = std::max(1.0, temperature + released / heatCapacity());

		// latent heat as substances change state
		double latent_heat = 0;
		for (size_t i = 0; i < amounts.size(); i++) {
			State now = state(i, c.pressure);
			if (now != m_states[i]) {
				latent_heat += amounts[i] * latent(subs[i], m_states[i], now);
				m_states[i] = now;
			}
		}
		temperature = std::max(1.0, temperature - latent_heat / heatCapacity());
		left -= dt;
	}
	for (double &d : done)
		d /= seconds;
	return done;
}

} // namespace matter
