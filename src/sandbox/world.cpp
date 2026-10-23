// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "world.h"
#include "sandbox.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace sandbox {

using matter::State;

static const double BLOCK = 1.0;          // m: a world block
static const double LONGEST_STEP = 1.0;   // s
static const double WAKE = 2.0;           // K: how far heat must warm (or chill) an asleep block to wake it
static const double SETTLED = 1.0;        // K from the weather's temperature that counts as settled
static const double QUIET = 20;           // s settled before a block sleeps
static const double GONE = 0.1;           // share of solid or liquid below which a block has become air
static const double NOTICED = 1e-4;       // reactions per second that count as happening (for discovery)
static const size_t MOST_AWAKE = 8192;
static const double SMOKE = 0.03;         // share of a block's gas, not the room's, that makes it smoke    // a guard: more awake blocks than this is a runaway

WorldMatter::WorldMatter(const Opts &opts) :
	m_matter(opts.matter), m_physics({ opts.matter, opts.pressure, BLOCK }), m_ground(opts.ground)
{
	if (!m_ground.kind || !m_ground.become || !m_ground.ambient)
		throw SandboxError("the world's matter needs the ground's kind, become and ambient");
	m_air.assign(m_matter.substances().size(), 0);
	double shares = 0;
	for (const auto &[id, share] : opts.air) {
		m_air[m_matter.index(id)] = share;
		shares += share;
	}
	if (std::fabs(shares - 1) > 1e-6)
		throw SandboxError("the room's gas shares must sum to 1");
}

int64_t WorldMatter::key(Pos p)
{
	return ((int64_t)(p.x & 0x1FFFFF) << 42) | ((int64_t)(p.y & 0x1FFFFF) << 21) | (int64_t)(p.z & 0x1FFFFF);
}

int32_t WorldMatter::kind(const Spec &spec)
{
	const std::string &node = spec.node, &id = spec.matter;
	double share = spec.share;
	State state = spec.state;
	if (m_by_node.count(node))
		throw SandboxError(node + " already has its matter registered");
	if (share <= 0 || share > 1)
		throw SandboxError(node + ": a block's matter takes 0..1 of its volume");
	Kind k{ node, {}, id == "air", spec.form, spec.flowing, -1, state, 0, 0, 0 };
	if (!k.air && state != State::Solid && state != State::Liquid)
		throw SandboxError(node + ": a block's matter is solid or liquid (gas fills the rest of it)");
	if (!k.air) {
		// the solid part: one substance or rock, or a mix of them; a rock is
		// its minerals, by volume from their shares by mass
		std::vector<std::pair<std::string, double>> parts = spec.parts;
		if (parts.empty())
			parts.push_back({ id, 1.0 });
		double shares = 0;
		for (const auto &[pid, part] : parts)
			shares += part;
		if (shares <= 0 || std::fabs(shares - 1) > 1e-3)
			throw SandboxError(node + ": a mix's shares must sum to 1");
		const auto &rocks = m_matter.rocks();
		for (const auto &[pid, part] : parts) {
			auto rock = std::find_if(rocks.begin(), rocks.end(), [&](const matter::RockKind &r) { return r.id == pid; });
			if (rock != rocks.end()) {
				double total = 0;
				for (const auto &p : rock->minerals)
					total += p.share / m_matter.substances()[p.substance].density_solid;
				for (const auto &p : rock->minerals)
					k.by_volume.push_back({ p.substance, share * part * p.share / m_matter.substances()[p.substance].density_solid / total });
			} else {
				k.by_volume.push_back({ m_matter.index(pid), share * part }); // throws on an unknown id
			}
		}
	}
	matter::Mixture sample = made(k, 293.15);
	Physics::Main main = m_physics.main(sample);
	k.main = main.substance;
	k.fill = main.fill;
	k.k = m_physics.conductivity(sample);
	k.capacity = sample.heatCapacity();
	m_kinds.push_back(k);
	m_by_node[node] = (int32_t)m_kinds.size() - 1;
	return (int32_t)m_kinds.size() - 1;
}

const std::string &WorldMatter::node(int32_t kind) const
{
	if (kind < 0 || (size_t)kind >= m_kinds.size())
		throw SandboxError("no such kind of block");
	return m_kinds[kind].node;
}

matter::Mixture WorldMatter::made(const Kind &k, double temperature) const
{
	matter::Mixture b(m_matter);
	b.vessel = 1e-3;
	m_physics.fill(b, k.by_volume, temperature, m_air, k.air ? std::nullopt : std::optional<State>(k.state));
	return b;
}

double WorldMatter::wakeAt(const Kind &k, double ambient) const
{
	if (k.air || k.main < 0)
		return ambient;
	// ice can't be at a summer's day temperature, nor water in a frost: they sit at their melting point
	const matter::Substance &s = m_matter.substances()[k.main];
	State there = s.state(ambient, std::max(m_physics.pressure(), 1.0));
	if (there == k.state)
		return ambient;
	if ((k.state == State::Solid && there != State::Solid) || (k.state == State::Liquid && there == State::Solid))
		return s.meltingPoint(std::max(m_physics.pressure(), s.triple.p));
	return s.boilingPoint(m_physics.pressure()).value_or(ambient); // a liquid where it would boil
}

static const double FALLING = 4; // per s: how fast a liquid runs into space below (as the sandbox's)

// A block's liquids: down into space below (an awake block's free volume, or
// asleep air, which wakes), then, if it holds more than its volume (zinc
// grows as it melts), the excess out into the air beside it.
void WorldMatter::pour(Cell &c, double dt)
{
	double volume = m_physics.volume();
	auto litres = [&](const matter::Mixture &b, size_t s) {
		const matter::Substance &sub = m_matter.substances()[s];
		return b.in(s, State::Liquid) * sub.molar_mass / 1000.0 / sub.density(State::Liquid, b.temperature, m_physics.pressure());
	};
	// a neighbour that can take liquid: awake with room, or asleep air (woken)
	auto room = [&](Pos p) -> Cell * {
		auto it = m_cells.find(key(p));
		if (it != m_cells.end())
			return m_physics.condensed(it->second.mix) < volume * 0.99 ? &it->second : nullptr;
		int32_t kind = m_ground.kind(p);
		return kind >= 0 && m_kinds[kind].air ? wake(p, m_ground.ambient(p)) : nullptr;
	};
	for (size_t s = 0; s < c.mix.amounts.size(); s++) {
		if (c.mix.in(s, State::Liquid) <= 0)
			continue;
		if (Cell *below = room({ c.pos.x, c.pos.y - 1, c.pos.z })) {
			double space = volume - m_physics.condensed(below->mix);
			double v = std::min(litres(c.mix, s), space) * std::min(1.0, FALLING * dt);
			if (v > 0)
				m_physics.move(c.mix, below->mix, s, State::Liquid, c.mix.in(s, State::Liquid) * v / litres(c.mix, s));
		}
		double over = m_physics.condensed(c.mix) - volume;
		if (over <= 0 || c.mix.in(s, State::Liquid) <= 0)
			continue;
		for (const Pos &d : { Pos{ 1, 0, 0 }, Pos{ -1, 0, 0 }, Pos{ 0, 0, 1 }, Pos{ 0, 0, -1 } }) {
			Cell *side = room({ c.pos.x + d.x, c.pos.y, c.pos.z + d.z });
			if (!side || over <= 0)
				continue;
			double v = std::min({ over, litres(c.mix, s), volume - m_physics.condensed(side->mix) });
			if (v > 0)
				m_physics.move(c.mix, side->mix, s, State::Liquid, c.mix.in(s, State::Liquid) * v / litres(c.mix, s));
			over -= v;
		}
	}
}

bool WorldMatter::same(const Kind &a, const Kind &b)
{
	return a.air == b.air && a.main == b.main && a.state == b.state && std::fabs(a.fill - b.fill) < 0.05;
}

WorldMatter::Cell *WorldMatter::wake(Pos at, double temperature)
{
	int32_t kind = m_ground.kind(at);
	auto it = m_cells.find(key(at));
	if (it != m_cells.end()) {
		if (kind == it->second.kind || (kind >= 0 && same(m_kinds[kind], m_kinds[it->second.kind])))
			return &it->second;
		m_cells.erase(it); // the block was replaced (a hot item placed where air was awake): start over as what's there
	}
	if (kind < 0)
		return nullptr;
	if (m_cells.size() >= MOST_AWAKE)
		throw SandboxError("more than 8192 blocks awake: a runaway");
	const Kind &k = m_kinds[kind];
	double base = wakeAt(k, m_ground.ambient(at));
	// woken by heat or chill: as far from where it was as that took it
	Cell c{ at, kind, made(k, base + (temperature - m_ground.ambient(at))) };
	m_pending.erase(key(at));
	return &m_cells.emplace(key(at), std::move(c)).first->second;
}

void WorldMatter::heat(Pos at, double temperature)
{
	if (temperature <= 0)
		throw SandboxError("a temperature must be above 0 K");
	Cell *c = wake(at, m_ground.ambient(at));
	if (c)
		c->mix.temperature = std::max(c->mix.temperature, temperature);
}

void WorldMatter::touch(Pos at, double temperature)
{
	if (temperature <= 0)
		throw SandboxError("a temperature must be above 0 K");
	Cell *c = wake(at, m_ground.ambient(at));
	if (c) {
		c->mix.temperature = temperature;
		c->quiet = 0;
	}
}

void WorldMatter::blow(Pos at, double moles)
{
	Cell *c = wake(at, m_ground.ambient(at));
	if (!c || moles <= 0)
		return;
	double ambient = m_ground.ambient(at), per_kelvin = 0;
	for (size_t s = 0; s < m_air.size(); s++) {
		const matter::Substance &sub = m_matter.substances()[s];
		per_kelvin += m_air[s] * moles * sub.molar_mass / 1000.0 * sub.heat_capacity;
		c->mix.add(s, State::Gas, m_air[s] * moles);
	}
	double capacity = c->mix.heatCapacity() - per_kelvin;
	c->mix.temperature = (capacity * c->mix.temperature + per_kelvin * ambient) / (capacity + per_kelvin);
	c->quiet = 0;
}

static const Pos SIDES[6] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };

// One awake block against its asleep neighbours: heat into solid ones (waking
// them once it matters), and the room's air through air ones.
void WorldMatter::exchange(Cell &c, double k, double dt)
{
	for (const Pos &d : SIDES) {
		Pos n{ c.pos.x + d.x, c.pos.y + d.y, c.pos.z + d.z };
		if (m_cells.count(key(n)))
			continue;
		int32_t kind = m_ground.kind(n);
		if (kind < 0)
			continue;
		const Kind &nk = m_kinds[kind];
		double ambient = m_ground.ambient(n);
		if (nk.air) {
			m_physics.lose(c.mix, 1, ambient, dt);
			m_physics.breathe(c.mix, m_air, ambient, d.y > 0, dt);
			continue;
		}
		double keff = k + nk.k > 0 ? 2 * k * nk.k / (k + nk.k) : 0;
		double q = keff * m_physics.area() / m_physics.block() * (c.mix.temperature - ambient) * dt; // J into the neighbour
		c.mix.temperature = std::max(1.0, c.mix.temperature - q / c.mix.heatCapacity());
		q += m_physics.radiateInto(c.mix, ambient, dt); // a flame shining on it
		double &pending = m_pending[key(n)];
		pending += q;
		if (std::fabs(pending) / nk.capacity > WAKE) {
			double warmed = ambient + pending / nk.capacity;
			m_pending.erase(key(n));
			wake(n, std::max(1.0, warmed));
		}
	}
}

// Change the block to the form of what now fills it, once its own kind no
// longer matches (a birch log stays a birch log until it's charcoal).
void WorldMatter::reshape(Cell &c)
{
	Physics::Main m = m_physics.main(c.mix);
	auto matches = [&](const Kind &k) {
		return m.fill < GONE ? k.air : (!k.air && k.main == m.substance && k.state == m.state);
	};
	if (matches(m_kinds[c.kind]))
		return;
	for (size_t i = 0; i < m_kinds.size(); i++)
		if (m_kinds[i].form && matches(m_kinds[i])) {
			m_ground.become(c.pos, (int32_t)i);
			c.kind = (int32_t)i;
			return;
		}
	// no form for what fills it: the block stays as it is, awake, until one is registered
}

double WorldMatter::unlike(const matter::Mixture &b) const
{
	double gas = m_physics.gasMoles(b), most = 0;
	if (gas <= 0)
		return 0;
	for (size_t s = 0; s < m_air.size(); s++)
		most = std::max(most, b.in(s, State::Gas) / gas - m_air[s]);
	return most;
}

bool WorldMatter::settled(const Cell &c) const
{
	const Kind &k = m_kinds[c.kind];
	Physics::Main m = m_physics.main(c.mix);
	if (k.air) { // air settles once its gas is the room's again
		double gas = m_physics.gasMoles(c.mix);
		for (size_t s = 0; s < m_air.size(); s++)
			if (gas > 0 && std::fabs(c.mix.in(s, State::Gas) / gas - m_air[s]) > 0.02)
				return false;
		return m.fill < GONE;
	}
	// close enough to sleep as the kind it is (melted ice takes 8% less room than the ice did)
	return m.substance == k.main && m.state == k.state && std::fabs(m.fill - k.fill) < 0.1;
}

void WorldMatter::step(double seconds)
{
	if (seconds <= 0)
		throw SandboxError("steps must move forward");
	const size_t reactions = m_matter.reactions().size();
	double left = seconds;
	while (left > 1e-9 && !m_cells.empty()) {
		double dt = std::min(left, LONGEST_STEP);
		// a block changed under us (dug, built over, flowed into) is no longer what we were running;
		// one swapped for another kind of the same matter (air for a flame's light) carries on
		for (auto it = m_cells.begin(); it != m_cells.end();) {
			int32_t now = m_ground.kind(it->second.pos);
			if (now == it->second.kind) {
				++it;
			} else if (now >= 0 && same(m_kinds[now], m_kinds[it->second.kind])) {
				it->second.kind = now;
				++it;
			} else {
				m_seen.erase(it->first);
				it = m_cells.erase(it);
			}
		}
		if (m_cells.empty())
			break;
		std::unordered_map<int64_t, double> k;
		for (auto &[id, c] : m_cells) {
			k[id] = m_physics.conductivity(c.mix);
			dt = std::min(dt, std::max(1e-3, 0.25 * m_physics.stable(c.mix, k[id])));
		}
		std::vector<double> rates;
		for (auto &[id, c] : m_cells) {
			c.activity = m_physics.react(c.mix, dt, &rates);
			std::vector<bool> &seen = m_seen[id];
			seen.resize(reactions, false);
			for (size_t j = 0; j < reactions; j++)
				if (rates[j] > NOTICED && !seen[j]) {
					seen[j] = true;
					m_happened.push_back({ c.pos, j });
				}
		}
		// awake neighbours with each other (each pair once), then with the asleep
		std::vector<int64_t> ids;
		for (const auto &[id, c] : m_cells)
			ids.push_back(id);
		std::sort(ids.begin(), ids.end()); // a fixed order: deterministic
		for (int64_t id : ids) {
			Cell &c = m_cells.at(id);
			for (const Pos &d : { Pos{ 1, 0, 0 }, Pos{ 0, 1, 0 }, Pos{ 0, 0, 1 } }) {
				auto it = m_cells.find(key({ c.pos.x + d.x, c.pos.y + d.y, c.pos.z + d.z }));
				if (it == m_cells.end())
					continue;
				m_physics.conduct(c.mix, it->second.mix, k.at(id), k.at(it->first), dt);
				m_physics.radiate(c.mix, it->second.mix, dt);
				m_physics.mix(c.mix, it->second.mix, d.y > 0, dt);
			}
		}
		for (int64_t id : ids) {
			auto it = m_cells.find(id);
			if (it != m_cells.end())
				exchange(it->second, k.at(id), dt);
		}
		// gas a block can't hold leaves upward (or sideways) into air; space left draws it in
		for (int64_t id : ids) {
			Cell &c = m_cells.at(id);
			for (const Pos &d : { Pos{ 0, 1, 0 }, Pos{ 1, 0, 0 }, Pos{ -1, 0, 0 }, Pos{ 0, 0, 1 }, Pos{ 0, 0, -1 }, Pos{ 0, -1, 0 } }) {
				Pos n{ c.pos.x + d.x, c.pos.y + d.y, c.pos.z + d.z };
				auto it = m_cells.find(key(n));
				if (it != m_cells.end() && m_physics.room(it->second.mix) > 0) {
					m_physics.balance(c.mix, it->second.mix);
					break;
				}
				int32_t kind = it == m_cells.end() ? m_ground.kind(n) : -1;
				if (kind >= 0 && m_kinds[kind].air) {
					// gas pushed out that isn't the room's air (smoke, a fire's fuel gas) wakes the
					// air it flows into, so it can burn there as a flame instead of vanishing
					if (m_physics.gasMoles(c.mix) > m_physics.room(c.mix) * 1.0001 && unlike(c.mix) > SMOKE) {
						Cell *air = wake(n, m_ground.ambient(n));
						if (air) {
							m_physics.balance(c.mix, air->mix);
							break;
						}
					}
					m_physics.balance(c.mix, m_air, m_ground.ambient(n));
					break;
				}
			}
		}
		// blocks become what fills them (ice that melted is water now)
		for (int64_t id : ids) {
			auto it = m_cells.find(id);
			if (it != m_cells.end())
				reshape(it->second);
		}
		// liquids fall into space below, and a block too full spills into the air beside it
		for (int64_t id : ids) {
			auto it = m_cells.find(id);
			if (it != m_cells.end() && !m_kinds[it->second.kind].flowing)
				pour(it->second, dt);
		}
		// settled blocks sleep
		for (int64_t id : ids) {
			auto it = m_cells.find(id);
			if (it == m_cells.end())
				continue;
			Cell &c = it->second;
			bool calm = std::fabs(c.mix.temperature - m_ground.ambient(c.pos)) < SETTLED && std::fabs(c.activity) < 0.01;
			c.quiet = calm ? c.quiet + dt : 0;
			if (c.quiet > QUIET && settled(c)) {
				m_seen.erase(id);
				m_cells.erase(it);
			}
		}
		left -= dt;
	}
}

double WorldMatter::capacity(Pos at) const
{
	auto it = m_cells.find(key(at));
	if (it != m_cells.end())
		return it->second.mix.heatCapacity();
	int32_t kind = m_ground.kind(at);
	return kind < 0 ? -1 : m_kinds[kind].capacity;
}

double WorldMatter::temperature(Pos at) const
{
	auto it = m_cells.find(key(at));
	return it != m_cells.end() ? it->second.mix.temperature : m_ground.ambient(at);
}

std::vector<WorldMatter::Awake> WorldMatter::awake(Pos lo, Pos hi) const
{
	std::vector<Awake> out;
	for (const auto &[id, c] : m_cells) {
		const Pos &p = c.pos;
		if (p.x < lo.x || p.y < lo.y || p.z < lo.z || p.x > hi.x || p.y > hi.y || p.z > hi.z)
			continue;
		Physics::Main m = m_physics.main(c.mix);
		Awake a{ p, c.mix.temperature, c.activity, m.substance, m.state, m.fill, -1, 0 };
		double gas = m_physics.gasMoles(c.mix), unlike = 0;
		if (gas > 0)
			for (size_t s = 0; s < m_air.size(); s++) {
				double excess = c.mix.in(s, State::Gas) / gas - m_air[s];
				if (c.mix.amounts[s] > 0 && excess > unlike) {
					unlike = excess;
					a.vapour = (int32_t)s;
					a.haze = std::min(1.0, excess);
				}
			}
		out.push_back(a);
	}
	return out;
}

std::optional<WorldMatter::Awake> WorldMatter::inspect(Pos at) const
{
	if (m_cells.count(key(at))) {
		std::vector<Awake> one = awake(at, at);
		return one.front();
	}
	int32_t kind = m_ground.kind(at);
	if (kind < 0)
		return std::nullopt;
	const Kind &k = m_kinds[kind];
	return Awake{ at, wakeAt(k, m_ground.ambient(at)), 0, k.air ? -1 : k.main, k.state, k.air ? 0 : k.fill, -1, 0 };
}

std::vector<std::pair<Pos, size_t>> WorldMatter::happened()
{
	std::vector<std::pair<Pos, size_t>> out;
	out.swap(m_happened);
	return out;
}

// ---- saving: the awake blocks, by node name so the registry's order doesn't matter

static const char MAGIC[8] = { 'T', 'F', 'W', 'M', 0, 0, 0, 1 };

template <typename T>
static void put(std::string &out, const T &v)
{
	out.append(reinterpret_cast<const char *>(&v), sizeof v);
}

template <typename T>
static T take(const std::string &in, size_t &at)
{
	if (at + sizeof(T) > in.size())
		throw SandboxError("world matter: cut short");
	T v;
	std::memcpy(&v, in.data() + at, sizeof v);
	at += sizeof v;
	return v;
}

std::string WorldMatter::save() const
{
	std::string out(MAGIC, sizeof MAGIC);
	put(out, (uint32_t)m_matter.substances().size());
	put(out, (uint32_t)m_cells.size());
	std::vector<int64_t> ids;
	for (const auto &[id, c] : m_cells)
		ids.push_back(id);
	std::sort(ids.begin(), ids.end());
	for (int64_t id : ids) {
		const Cell &c = m_cells.at(id);
		put(out, c.pos);
		const std::string &name = m_kinds[c.kind].node;
		put(out, (uint32_t)name.size());
		out += name;
		put(out, c.mix.temperature);
		for (double n : c.mix.amounts)
			put(out, n);
	}
	return out;
}

void WorldMatter::load(const std::string &in)
{
	if (in.size() < sizeof MAGIC || std::memcmp(in.data(), MAGIC, sizeof MAGIC) != 0)
		throw SandboxError("world matter: not a world matter file (or an older version)");
	size_t at = sizeof MAGIC;
	if (take<uint32_t>(in, at) != m_matter.substances().size())
		throw SandboxError("world matter: saved with another matter table");
	uint32_t count = take<uint32_t>(in, at);
	std::unordered_map<int64_t, Cell> cells;
	for (uint32_t k = 0; k < count; k++) {
		Pos p = take<Pos>(in, at);
		uint32_t length = take<uint32_t>(in, at);
		if (at + length > in.size())
			throw SandboxError("world matter: cut short");
		std::string name = in.substr(at, length);
		at += length;
		auto kind = m_by_node.find(name);
		if (kind == m_by_node.end())
			throw SandboxError("world matter: a block of " + name + ", which has no matter registered now");
		Cell c{ p, kind->second, made(m_kinds[kind->second], 293.15) };
		double t = take<double>(in, at);
		for (size_t s = 0; s < c.mix.amounts.size(); s++) {
			double n = take<double>(in, at);
			for (State st : { State::Solid, State::Liquid, State::Gas })
				c.mix.take(s, st, c.mix.amounts[s]);
			c.mix.temperature = t;
			if (n > 0)
				c.mix.add(s, m_matter.substances()[s].state(t, std::max(m_physics.pressure(), 1.0)), n);
		}
		cells.emplace(key(p), std::move(c));
	}
	if (at != in.size())
		throw SandboxError("world matter: trailing bytes");
	m_cells = std::move(cells);
}

} // namespace sandbox
