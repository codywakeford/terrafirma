// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "test.h"

#include "porting.h"
#include "sandbox/sandbox.h"
#include "sandbox/world.h"
#include <map>
#include <tuple>
#include <cmath>

using namespace sandbox;

class TestSandbox : public TestBase {
public:
	TestSandbox() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestSandbox"; }

	void runTests(IGameDef *gamedef);

	void testRejects();
	void testConduction();
	void testMeltingAndPouring();
	void testFireNeedsAir();
	void testGasVents();
	void testDeterministic();
	void testWorldFire();
	void testWorldWalls();
	void testWorldMeltsAndSleeps();
	void testWorldSaves();
	void testWorldPours();
};

static TestSandbox g_test_instance;

static const matter::Matter &table()
{
	static matter::Matter m = matter::Matter::load(porting::path_share + DIR_DELIM "matter");
	return m;
}

static const Parts AIR = { { "nitrogen", 0.79 }, { "oxygen", 0.21 } };

static Opts box(int32_t x, int32_t y, int32_t z, double block = 0.1, Parts air = AIR)
{
	return Opts{ table(), x, y, z, block, 293.15, 101325, air };
}

static double total(const Sandbox &s, const char *id)
{
	size_t k = table().index(id);
	double n = 0;
	for (int32_t y = 0; y < s.sizeY(); y++)
		for (int32_t z = 0; z < s.sizeZ(); z++)
			for (int32_t x = 0; x < s.sizeX(); x++)
				n += s.block({ x, y, z }).amounts[k];
	return n;
}

void TestSandbox::runTests(IGameDef *gamedef)
{
	TEST(testRejects);
	TEST(testConduction);
	TEST(testMeltingAndPouring);
	TEST(testFireNeedsAir);
	TEST(testGasVents);
	TEST(testDeterministic);
	TEST(testWorldFire);
	TEST(testWorldWalls);
	TEST(testWorldMeltsAndSleeps);
	TEST(testWorldSaves);
	TEST(testWorldPours);
}

void TestSandbox::testRejects()
{
	EXCEPTION_CHECK(SandboxError, Sandbox(box(0, 1, 1)));
	EXCEPTION_CHECK(SandboxError, Sandbox(box(17, 1, 1)));
	Sandbox s(box(2, 2, 2));
	EXCEPTION_CHECK(SandboxError, s.fill({ 2, 0, 0 }, {}, 300));
	EXCEPTION_CHECK(SandboxError, s.fill({ 0, 0, 0 }, { { "iron", 0.7 }, { "copper", 0.7 } }, 300));
	EXCEPTION_CHECK(SandboxError, s.fill({ 0, 0, 0 }, { { "oxygen", 1 } }, 300)); // a gas, not a fill
	EXCEPTION_CHECK(matter::MatterError, s.fill({ 0, 0, 0 }, { { "unobtainium", 1 } }, 300));
	EXCEPTION_CHECK(SandboxError, s.step(0));
}

// A rod of copper and a rod of wood, each heated at one end: heat runs down
// the copper far faster.
void TestSandbox::testConduction()
{
	Sandbox s(box(2, 1, 6, 0.02));
	for (int32_t z = 0; z < 6; z++) {
		s.fill({ 0, 0, z }, { { "copper", 1 } }, 293.15);
		s.fill({ 1, 0, z }, { { "wood", 1 } }, 293.15);
	}
	s.heater({ 0, 0, 0 }, 20);
	s.heater({ 1, 0, 0 }, 20);
	s.step(60);
	double copper = s.block({ 0, 0, 5 }).temperature, wood = s.block({ 1, 0, 5 }).temperature;
	infostream << "sandbox: far end after 60 s, copper " << copper << " K, wood " << wood << " K" << std::endl;
	UASSERT(copper - 293.15 > 20 * (wood - 293.15)); // copper carries it; wood barely does
	UASSERT(s.block({ 1, 0, 0 }).temperature > s.block({ 0, 0, 0 }).temperature); // the wood keeps its heat where it's put
}

// Ice in a warm room melts, and the water runs down to the bottom.
void TestSandbox::testMeltingAndPouring()
{
	Sandbox s(box(1, 3, 1, 0.02));
	s.fill({ 0, 2, 0 }, { { "water", 0.5 } }, 263);
	size_t water = table().index("water");
	UASSERT(s.block({ 0, 2, 0 }).amounts[water] > 0);
	s.heater({ 0, 2, 0 }, 30);
	for (int k = 0; k < 120 && s.block({ 0, 0, 0 }).amounts[water] < 1e-3; k++)
		s.step(5);
	infostream << "sandbox: water at the bottom " << s.block({ 0, 0, 0 }).amounts[water] << " mol after " << s.time() << " s" << std::endl;
	UASSERT(s.block({ 0, 0, 0 }).amounts[water] > 1e-3);
	std::vector<BlockView> v = s.view();
	UASSERT(v[0].main == (int32_t)water && v[0].state == matter::State::Liquid);
}

// Wood lit in air burns: oxygen goes, carbon dioxide comes. Lit in pure
// nitrogen it only chars.
void TestSandbox::testFireNeedsAir()
{
	struct Fire { double co2, charcoal, temperature; };
	auto light = [](Parts air) {
		Sandbox s(box(3, 3, 3, 0.05, air));
		s.fill({ 1, 0, 1 }, { { "wood", 0.6 } }, 293.15);
		s.touch({ 1, 0, 1 }, 900);
		s.heater({ 1, 0, 1 }, 200);
		s.step(30);
		return Fire{ total(s, "carbon_dioxide"), total(s, "carbon"), s.block({ 1, 0, 1 }).temperature };
	};
	Fire air = light(AIR), nitrogen = light({ { "nitrogen", 1 } });
	infostream << "sandbox: in air CO₂ " << air.co2 << " mol at " << air.temperature << " K; in nitrogen CO₂ "
			<< nitrogen.co2 << " mol, charcoal " << nitrogen.charcoal << " mol at " << nitrogen.temperature << " K" << std::endl;
	UASSERT(air.co2 > 1e-4);                             // burning (most of it has already left by the top)
	UASSERT(air.temperature > nitrogen.temperature + 50); // and the fire's own heat
	UASSERT(nitrogen.co2 < 1e-9);                        // no air, no fire
	UASSERT(nitrogen.charcoal > 1e-3);                   // only charring
}

// Carbon dioxide released at the bottom of an open box leaves by the top.
void TestSandbox::testGasVents()
{
	Sandbox s(box(1, 4, 1));
	s.gas({ 0, 0, 0 }, { { "carbon_dioxide", 1 } }, 293.15);
	double start = total(s, "carbon_dioxide");
	s.step(600);
	double now = total(s, "carbon_dioxide");
	infostream << "sandbox: CO₂ " << start << " → " << now << " mol" << std::endl;
	UASSERT(now < start * 0.8);
	UASSERT(s.block({ 0, 3, 0 }).amounts[table().index("carbon_dioxide")] > 0); // it passed through the top
}

void TestSandbox::testDeterministic()
{
	auto run = [] {
		Sandbox s(box(3, 2, 3, 0.05));
		s.fill({ 1, 0, 1 }, { { "wood", 0.5 } }, 293.15);
		s.touch({ 1, 0, 1 }, 900);
		s.step(10);
		std::vector<double> out;
		for (const BlockView &v : s.view())
			out.push_back(v.temperature);
		return out;
	};
	UASSERT(run() == run());
}

// ---- the world's own blocks (WorldMatter), over a little plot of node names

namespace {

struct Plot
{
	std::map<std::tuple<int, int, int>, std::string> nodes; // anything missing is air
	double ambient = 293.15;
	std::unique_ptr<WorldMatter> matter;
	std::map<std::string, int32_t> kinds;

	Plot()
	{
		WorldMatter::Ground g{
			[this](Pos p) {
				auto it = nodes.find({ p.x, p.y, p.z });
				return kinds.at(it == nodes.end() ? "air" : it->second);
			},
			[this](Pos p, int32_t kind) { nodes[{ p.x, p.y, p.z }] = matter->node(kind); },
			[this](Pos) { return ambient; },
		};
		matter = std::make_unique<WorldMatter>(WorldMatter::Opts{ table(), 101325, AIR, g });
		using matter::State;
		for (auto [node, id, share, state, form] : std::vector<std::tuple<const char *, const char *, double, State, bool>>{
				{ "air", "air", 1, State::Gas, true }, { "log", "wood", 0.4, State::Solid, true },
				{ "charcoal", "carbon", 0.15, State::Solid, true }, { "clay", "kaolinite", 0.6, State::Solid, true },
				{ "granite", "granite", 0.99, State::Solid, true }, { "water", "water", 1, State::Liquid, true },
				{ "ice", "water", 1, State::Solid, true }, { "tin", "tin", 1, State::Solid, true },
				{ "tin_liquid", "tin", 1, State::Liquid, true } })
			kinds[node] = matter->kind({ node, id, {}, share, state, form, std::string(node) == "water" }); // water flows by the engine, as in the game
	}
	std::string at(int x, int y, int z) const
	{
		auto it = nodes.find({ x, y, z });
		return it == nodes.end() ? "air" : it->second;
	}
	// a hole: walls of `wall` around and under (0, 0, 0), open above
	void hole(const std::string &wall)
	{
		for (int x = -1; x <= 1; x++)
			for (int z = -1; z <= 1; z++) {
				nodes[{ x, -1, z }] = wall;
				if (x || z)
					nodes[{ x, 0, z }] = wall;
			}
	}
};

// The hottest any block gets over `seconds` (the flame, mostly), stepping a second at a time.
double peak(Plot &m, double seconds)
{
	double hottest = 0;
	for (double t = 0; t < seconds; t += 1) {
		m.matter->step(1);
		for (const WorldMatter::Awake &a : m.matter->awake({ -9, -9, -9 }, { 9, 9, 9 }))
			hottest = std::max(hottest, a.temperature);
	}
	return hottest;
}

} // namespace

// A log lit in a hole burns: hot, then charcoal, then gone to air; the
// ground around warms and wakes.
void TestSandbox::testWorldFire()
{
	Plot m;
	m.hole("granite");
	m.nodes[{ 0, 0, 0 }] = "log";
	m.matter->heat({ 0, 0, 0 }, 900);
	double hottest = peak(m, 900);
	infostream << "world: the fire peaked at " << hottest << " K; the log is now " << m.at(0, 0, 0)
			<< "; " << m.matter->awakeCount() << " blocks awake" << std::endl;
	UASSERT(hottest > 1000);                   // a flame over it
	UASSERT(m.matter->awakeCount() > 1);       // and warmed the ground around it
	// buried with no air, the same log only chars
	Plot buried;
	buried.hole("granite");
	buried.nodes[{ 0, 1, 0 }] = "granite";
	buried.nodes[{ 0, 0, 0 }] = "log";
	buried.matter->heat({ 0, 0, 0 }, 900);
	double smothered = peak(buried, 900);
	infostream << "world: buried, it peaked at " << smothered << " K" << std::endl;
	UASSERT(smothered < hottest - 100);
}

// Clay insulates: a hot core walled in clay keeps its heat longer than one
// walled in granite, which conducts about three times as well.
void TestSandbox::testWorldWalls()
{
	Plot clay, stone;
	for (auto [m, wall] : { std::pair<Plot *, const char *>{ &clay, "clay" }, { &stone, "granite" } }) {
		for (int x = -1; x <= 1; x++)
			for (int y = -1; y <= 1; y++)
				for (int z = -1; z <= 1; z++)
					m->nodes[{ x, y, z }] = (x || y || z) ? wall : "granite";
		m->matter->heat({ 0, 0, 0 }, 1000);
		for (int k = 0; k < 1800; k++)
			m->matter->step(1);
	}
	double in_clay = clay.matter->temperature({ 0, 0, 0 }), in_stone = stone.matter->temperature({ 0, 0, 0 });
	infostream << "world: a 1000 K core after 30 min, walled in clay " << in_clay << " K, in granite " << in_stone << " K" << std::endl;
	UASSERT(in_clay > in_stone + 2); // a cubic metre of hot granite cools slowly either way: the walls change how slowly
}

// Ice warmed past 0 °C becomes water (the block changes to water's form);
// left alone, everything settles and sleeps.
void TestSandbox::testWorldMeltsAndSleeps()
{
	Plot m;
	m.nodes[{ 0, 0, 0 }] = "ice";
	m.matter->heat({ 0, 0, 0 }, 360); // a cubic metre of ice: warming it this far melts it all (and not past boiling)
	for (int k = 0; k < 4000 && m.at(0, 0, 0) == "ice"; k++)
		m.matter->step(1);
	UASSERT(m.at(0, 0, 0) == "water");
	// a cubic metre of water takes a day to warm to the room: let the weather be its temperature, and it settles
	m.ambient = m.matter->temperature({ 0, 0, 0 });
	for (int k = 0; k < 2000 && m.matter->awakeCount(); k++)
		m.matter->step(1);
	UASSERT(m.matter->awakeCount() == 0);
	UASSERT(m.matter->temperature({ 0, 0, 0 }) == m.ambient); // asleep: the weather's temperature
}

void TestSandbox::testWorldSaves()
{
	Plot a;
	a.hole("granite");
	a.nodes[{ 0, 0, 0 }] = "log";
	a.matter->heat({ 0, 0, 0 }, 900);
	a.matter->step(20);
	Plot b;
	b.nodes = a.nodes;
	b.matter->load(a.matter->save());
	UASSERT(b.matter->save() == a.matter->save());
	a.matter->step(5);
	b.matter->step(5);
	UASSERT(std::fabs(a.matter->temperature({ 0, 0, 0 }) - b.matter->temperature({ 0, 0, 0 })) < 1e-6);
	EXCEPTION_CHECK(SandboxError, b.matter->load("nonsense"));
}

// Tin melted on a shelf runs down into the space below it.
void TestSandbox::testWorldPours()
{
	Plot m;
	m.nodes[{ 0, -2, 0 }] = "granite";
	m.nodes[{ 0, 0, 0 }] = "tin";
	m.matter->heat({ 0, 0, 0 }, 650); // tin melts at 505 K
	for (int k = 0; k < 120 && m.at(0, -1, 0) == "air"; k++)
		m.matter->step(1);
	infostream << "world: after pouring, above " << m.at(0, 0, 0) << ", below " << m.at(0, -1, 0) << std::endl;
	UASSERT(m.at(0, -1, 0) == "tin_liquid" || m.at(0, -1, 0) == "tin"); // what melted ran down
	// (650 K only melts part of a cubic metre of tin before it drops to its melting point: the rest stays on the shelf)
}
