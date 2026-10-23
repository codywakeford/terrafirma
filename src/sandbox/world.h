// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "physics.h"
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

/*
	The world as a sandbox: its own blocks run the shared physics (physics.h)
	wherever something is happening — a fire in a hole, a clay kiln, lava
	meeting water — and sleep everywhere else.

	- Awake blocks hold their matter and temperature and run reactions,
	  conduction and gas exchange with their neighbours.
	- An asleep block is what its kind of block is made of, at the weather's
	  temperature. Heat flowing into a solid one wakes it once it has warmed
	  enough to matter; asleep air is the room (oxygen in, smoke out).
	- When an awake block's matter changes enough, the block changes: to the
	  kind registered as the form of what now fills it (wood → charcoal →
	  air, wet clay → fired clay, water → ice).
	- A settled block (at the weather's temperature, nothing reacting, its
	  matter what its kind is made of) goes back to sleep.

	The world itself is reached through `Ground`, so this has no engine
	dependencies (and tests run it over a map of their own).
*/
namespace sandbox {

struct Pos
{
	int32_t x, y, z;
};

class WorldMatter
{
public:
	// The world, as this sees it.
	struct Ground
	{
		std::function<int32_t(Pos)> kind;         // the registered kind at a position, −1 if none (it exchanges nothing)
		std::function<void(Pos, int32_t)> become; // make the block at a position this kind
		std::function<double(Pos)> ambient;       // K: the weather's temperature there
	};
	struct Opts
	{
		const matter::Matter &matter;
		double pressure;                         // Pa
		std::vector<std::pair<std::string, double>> air; // the room's gas by mole share
		Ground ground;
	};
	explicit WorldMatter(const Opts &opts);

	// A kind of block: what a node is made of.
	struct Spec
	{
		std::string node;
		std::string matter;    // a substance or rock id from the matter tables, or "air"
		// or a mix: substances and rocks, each with its share of the solid part
		// (a soil point is quartz, kaolinite and organic matter; a rock blend two rocks)
		std::vector<std::pair<std::string, double>> parts;
		double share = 1;      // of its volume (the rest is air in its pores: a log is ~40% wood)
		matter::State state = matter::State::Solid; // ice is water, solid
		bool form = false;     // the block that matter becomes when it fills one
		bool flowing = false;  // a liquid the engine flows itself (water, lava): not poured here
	};
	int32_t kind(const Spec &spec); // returns its id
	const std::string &node(int32_t kind) const;

	// Bring a block to at least a temperature (a spark, a torch): wakes it.
	void heat(Pos at, double temperature);
	// Set a block's temperature exactly (a heat gun, a quench): wakes it.
	void touch(Pos at, double temperature);
	// Push the room's air into a block (a bellows): moles now; wakes it.
	void blow(Pos at, double moles);
	// Step the awake blocks by dt seconds. A block whose kind has changed
	// underneath (dug, built over) is dropped first: the world wins.
	void step(double dt);

	double temperature(Pos at) const; // awake: its own; asleep: the weather's
	double capacity(Pos at) const;    // J/K: the heat its matter holds per kelvin (−1: no matter)
	struct Awake
	{
		Pos pos;
		double temperature, activity; // K, W of reaction heat (+ given off)
		int32_t main;                 // substance filling most of it, −1: only gas
		matter::State state;
		double fill;
		int32_t vapour;               // the gas most unlike the room's air, −1: none
		double haze;
	};
	std::vector<Awake> awake(Pos min, Pos max) const;
	// One block, awake or asleep (asleep: what its kind is, at the weather's
	// temperature); nullopt where there's no matter registered.
	std::optional<Awake> inspect(Pos at) const;
	size_t awakeCount() const { return m_cells.size(); }
	// Reactions that ran noticeably since the last call, and where (once each per block).
	std::vector<std::pair<Pos, size_t>> happened();

	std::string save() const;
	void load(const std::string &bytes); // throws SandboxError if it isn't this table's

private:
	struct Kind
	{
		std::string node;
		std::vector<std::pair<size_t, double>> by_volume; // solids at room temperature
		bool air, form, flowing;
		int32_t main;       // its main substance, for forms (−1 for air)
		matter::State state;
		double fill;
		double k, capacity; // asleep at the weather's temperature: W/(m·K), J/K
	};
	struct Cell
	{
		Pos pos;
		int32_t kind;
		matter::Mixture mix;
		double quiet = 0;    // s settled
		double activity = 0; // W
	};

	static int64_t key(Pos p);
	static bool same(const Kind &a, const Kind &b); // the same matter in the same state
	Cell *wake(Pos at, double temperature);
	matter::Mixture made(const Kind &k, double temperature) const;
	double wakeAt(const Kind &k, double ambient) const; // where an asleep block of a kind really is
	void exchange(Cell &c, double k, double dt);
	void reshape(Cell &c);
	void pour(Cell &c, double dt);
	bool settled(const Cell &c) const;
	double unlike(const matter::Mixture &b) const; // how far its gas is from the room's air (the largest excess share)

	const matter::Matter &m_matter;
	Physics m_physics;
	Ground m_ground;
	std::vector<double> m_air;
	std::vector<Kind> m_kinds;
	std::unordered_map<std::string, int32_t> m_by_node;
	std::unordered_map<int64_t, Cell> m_cells;
	std::unordered_map<int64_t, double> m_pending; // J flowed into asleep solid blocks, not yet enough to wake them
	std::vector<std::pair<Pos, size_t>> m_happened;
	std::unordered_map<int64_t, std::vector<bool>> m_seen; // per block, reactions already reported
};

} // namespace sandbox
