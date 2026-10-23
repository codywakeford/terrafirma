// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "matter.h"
#include <algorithm>
#include <cmath>
#include <fstream>

namespace matter {

const char *const STATE_NAMES[4] = { "solid", "liquid", "gas", "supercritical" };

static constexpr double R = 8.314462618; // J/(mol·K)

// ln p as a straight line in 1/T between two anchors (Clausius–Clapeyron
// with a constant latent heat over the span).
static double between(Point a, Point b, double t)
{
	double s = (std::log(b.p) - std::log(a.p)) / (1.0 / b.t - 1.0 / a.t);
	return std::exp(std::log(a.p) + s * (1.0 / t - 1.0 / a.t));
}

double Substance::vapourPressure(double t) const
{
	// through the triple point, the normal boiling point where there is one, and the critical point
	if (boil && *boil > triple.t && *boil < critical.t) {
		Point normal{ *boil, ATMOSPHERE };
		return t <= *boil ? between(triple, normal, t) : between(normal, critical, t);
	}
	return between(triple, critical, t);
}

double Substance::sublimationPressure(double t) const
{
	// through where it sublimes at 1 atm, if it does; otherwise from the
	// latent heats (sublimation: melting then boiling)
	if (sublime)
		return between(Point{ *sublime, ATMOSPHERE }, triple, t);
	double heat = (fusion + vaporization) * 1000.0;
	return triple.p * std::exp(-heat / R * (1.0 / t - 1.0 / triple.t));
}

double Substance::meltingPoint(double p) const
{
	// a straight line from the triple point; one that leans back (water) is
	// held at half the triple temperature, where real ices turn into denser
	// phases that this model doesn't have
	return std::max(triple.t + melt_slope * (p - triple.p) / 1e6, triple.t * 0.5);
}

std::optional<double> Substance::boilingPoint(double p) const
{
	if (p <= triple.p || p >= critical.p)
		return std::nullopt; // below the triple point it sublimes; above the critical, no boiling at all
	// invert the vapour-pressure curve: it rises steadily with temperature
	double lo = triple.t, hi = critical.t;
	for (int i = 0; i < 60; i++) {
		double mid = (lo + hi) / 2;
		(vapourPressure(mid) < p ? lo : hi) = mid;
	}
	return (lo + hi) / 2;
}

double Substance::density(State s, double t, double p) const
{
	switch (s) {
	case State::Solid: return density_solid;
	case State::Liquid: return density_liquid;
	case State::Gas:
	case State::Supercritical: return p * molar_mass / 1000.0 / (GAS_CONSTANT * t);
	}
	return density_liquid;
}

double Substance::conducts(State s) const
{
	return conductivity[s == State::Solid ? 0 : s == State::Liquid ? 1 : 2];
}

State Substance::state(double t, double p) const
{
	if (t >= critical.t)
		return p >= critical.p ? State::Supercritical : State::Gas;
	bool gas = t < triple.t ? p < sublimationPressure(t) : p < vapourPressure(t);
	if (gas)
		return State::Gas;
	// condensed: the melting line decides, wherever there's pressure enough
	// for a liquid (water's leans back, so squeezed ice melts below 0 °C)
	if (p <= triple.p)
		return State::Solid;
	return t < meltingPoint(p) ? State::Solid : State::Liquid;
}

static const Json::Value &field(const Json::Value &v, const char *name, const std::string &where)
{
	if (!v.isMember(name))
		throw MatterError(where + ": missing \"" + name + "\"");
	return v[name];
}

static double number(const Json::Value &v, const char *name, const std::string &where)
{
	const Json::Value &f = field(v, name, where);
	if (!f.isNumeric())
		throw MatterError(where + ": \"" + name + "\" must be a number");
	return f.asDouble();
}

static Point point(const Json::Value &v, const char *name, const std::string &where)
{
	const Json::Value &f = field(v, name, where);
	if (!f.isArray() || f.size() != 2 || !f[0].isNumeric() || !f[1].isNumeric() || f[0].asDouble() <= 0 || f[1].asDouble() <= 0)
		throw MatterError(where + ": \"" + name + "\" must be [kelvin, pascals], both positive");
	return { f[0].asDouble(), f[1].asDouble() };
}

static Json::Value readJson(const std::string &path)
{
	std::ifstream in(path);
	if (!in)
		throw MatterError("can't read " + path);
	Json::Value v;
	std::string errors;
	if (!Json::parseFromStream(Json::CharReaderBuilder(), in, &v, &errors))
		throw MatterError(path + " isn't JSON: " + errors);
	return v;
}

Matter Matter::load(const std::string &dir)
{
	Matter m;
	Json::Value elements = readJson(dir + "/elements.json");
	Json::Value substances = readJson(dir + "/substances.json");
	if (!elements["elements"].isArray() || !substances["substances"].isArray())
		throw MatterError(dir + ": elements.json needs \"elements\", substances.json \"substances\"");

	for (const Json::Value &e : elements["elements"]) {
		std::string where = "element " + e["symbol"].asString();
		m.m_elements.push_back({ (int)number(e, "number", where), field(e, "symbol", where).asString(),
				field(e, "name", where).asString(), field(e, "category", where).asString(), number(e, "mass", where) });
	}

	for (const Json::Value &s : substances["substances"]) {
		std::string where = "substance " + s["id"].asString();
		Substance x;
		x.id = field(s, "id", where).asString();
		x.name = field(s, "name", where).asString();
		x.formula = field(s, "formula", where).asString();
		x.molar_mass = 0;
		const Json::Value &comp = field(s, "composition", where);
		if (!comp.isObject() || comp.empty())
			throw MatterError(where + ": composition must name its elements");
		for (const std::string &symbol : comp.getMemberNames()) {
			int count = comp[symbol].asInt();
			if (count <= 0)
				throw MatterError(where + ": " + symbol + " count must be positive");
			x.composition[symbol] = count;
			x.molar_mass += count * m.element(symbol).mass;
		}
		x.triple = point(s, "triple", where);
		x.critical = point(s, "critical", where);
		if (!s["boil"].isNull())
			x.boil = number(s, "boil", where);
		x.fusion = number(s, "fusion", where);
		x.vaporization = number(s, "vaporization", where);
		x.melt_slope = number(s, "melt_slope", where);
		x.heat_capacity = number(s, "heat_capacity", where);
		if (x.heat_capacity <= 0)
			throw MatterError(where + ": heat_capacity must be positive");
		const Json::Value &density = field(s, "density", where);
		x.density_solid = number(density, "solid", where + ".density");
		x.density_liquid = number(density, "liquid", where + ".density");
		const Json::Value &conducts = field(s, "conductivity", where);
		const char *const STATES[] = { "solid", "liquid", "gas" };
		for (int k = 0; k < 3; k++)
			x.conductivity[k] = number(conducts, STATES[k], where + ".conductivity");
		for (double v : { x.density_solid, x.density_liquid, x.conductivity[0], x.conductivity[1], x.conductivity[2] })
			if (v <= 0)
				throw MatterError(where + ": densities and conductivities must be positive");
		x.approximate = s["approximate"].asBool();
		if (x.triple.t >= x.critical.t || x.triple.p >= x.critical.p)
			throw MatterError(where + ": the triple point must lie below the critical point");
		if (x.boil && (*x.boil <= x.triple.t || *x.boil >= x.critical.t))
			throw MatterError(where + ": the boiling point must lie between the triple and critical points");
		if (!s["sublime"].isNull())
			x.sublime = number(s, "sublime", where);
		if (x.boil.has_value() == x.sublime.has_value())
			throw MatterError(where + ": give a boiling point at 1 atm, or where it sublimes at 1 atm, not both");
		if (x.sublime && (x.triple.p <= ATMOSPHERE || *x.sublime >= x.triple.t))
			throw MatterError(where + ": a substance that sublimes at 1 atm has its triple point above 1 atm and its sublimation below it");
		if (x.boil && x.triple.p >= ATMOSPHERE)
			throw MatterError(where + ": a boiling point at 1 atm needs its triple point below 1 atm");
		m.m_substances.push_back(x);
	}
	Json::Value reactions = readJson(dir + "/reactions.json");
	if (!reactions["reactions"].isArray())
		throw MatterError(dir + ": reactions.json needs \"reactions\"");
	for (const Json::Value &r : reactions["reactions"]) {
		std::string where = "reaction " + r["id"].asString();
		Reaction x;
		x.id = field(r, "id", where).asString();
		x.name = field(r, "name", where).asString();
		x.heat = number(r, "heat", where);
		x.activation = number(r, "activation", where);
		x.factor = number(r, "factor", where);
		if (x.activation < 0 || x.factor <= 0)
			throw MatterError(where + ": activation must be ≥ 0 and factor positive");
		std::string energy = r["energy"].isNull() ? "" : r["energy"].asString();
		x.energy = energy.empty() ? Energy::None : energy == "electricity" ? Energy::Electricity
				: energy == "light" ? Energy::Light : throw MatterError(where + ": energy is electricity or light");
		if (x.energy != Energy::None && x.heat >= 0)
			throw MatterError(where + ": a reaction driven by electricity or light takes heat (negative heat)");

		// the atoms on each side, which must match
		std::map<std::string, double> atoms;
		auto terms = [&](const char *side, std::vector<Reaction::Term> &out, double sign) {
			const Json::Value &list = field(r, side, where);
			if (!list.isObject() || list.empty())
				throw MatterError(where + ": " + side + " must name substances and counts");
			for (const std::string &id : list.getMemberNames()) {
				double count = list[id].asDouble();
				if (count <= 0)
					throw MatterError(where + ": " + id + " count must be positive");
				size_t i = m.index(id);
				out.push_back({ i, count });
				for (const auto &[symbol, n] : m.m_substances[i].composition)
					atoms[symbol] += sign * n * count;
			}
		};
		terms("reactants", x.reactants, 1);
		terms("products", x.products, -1);
		for (const auto &[symbol, left] : atoms)
			if (std::fabs(left) > 1e-9)
				throw MatterError(where + ": doesn't balance — " + symbol + " is off by " + std::to_string(left));

		for (const Json::Value &q : r["requires"]) {
			std::string state = q["state"].asString();
			int found = -1;
			for (int k = 0; k < 4; k++)
				if (state == STATE_NAMES[k])
					found = k;
			if (found < 0)
				throw MatterError(where + ": a requirement's state must be solid, liquid, gas or supercritical");
			x.needs.push_back({ m.index(q["substance"].asString()), (State)found });
		}
		m.m_reactions.push_back(x);
	}

	Json::Value rocks = readJson(dir + "/rocks.json");
	if (!rocks["rocks"].isArray())
		throw MatterError(dir + ": rocks.json needs \"rocks\"");
	for (const Json::Value &r : rocks["rocks"]) {
		std::string where = "rock " + r["id"].asString();
		RockKind x;
		x.id = field(r, "id", where).asString();
		x.name = field(r, "name", where).asString();
		x.origin = field(r, "origin", where).asString();
		const Json::Value &minerals = field(r, "minerals", where);
		double total = 0;
		for (const std::string &id : minerals.getMemberNames()) {
			double share = minerals[id].asDouble();
			if (share <= 0)
				throw MatterError(where + ": " + id + " share must be positive");
			x.minerals.push_back({ m.index(id), share });
			total += share;
		}
		if (std::fabs(total - 1) > 1e-6)
			throw MatterError(where + ": its minerals' shares add up to " + std::to_string(total) + ", not 1");
		m.m_rocks.push_back(x);
	}

	m.m_raw["elements"] = elements["elements"];
	m.m_raw["rocks"] = rocks["rocks"];
	m.m_raw["substances"] = substances["substances"];
	m.m_raw["reactions"] = reactions["reactions"];
	return m;
}

const Substance &Matter::substance(const std::string &id) const
{
	for (const Substance &s : m_substances)
		if (s.id == id)
			return s;
	throw MatterError("no substance \"" + id + "\"");
}

size_t Matter::index(const std::string &id) const
{
	for (size_t i = 0; i < m_substances.size(); i++)
		if (m_substances[i].id == id)
			return i;
	throw MatterError("no substance \"" + id + "\"");
}

const Element &Matter::element(const std::string &symbol) const
{
	for (const Element &e : m_elements)
		if (e.symbol == symbol)
			return e;
	throw MatterError("no element \"" + symbol + "\"");
}

} // namespace matter
