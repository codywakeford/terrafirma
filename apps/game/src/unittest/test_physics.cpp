// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "test.h"

#include "physics/walk.h"
#include "physics/world.h"
#include <cmath>

using namespace physics;

class TestPhysics : public TestBase {
public:
	TestPhysics() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestPhysics"; }

	void runTests(IGameDef *gamedef);

	void testBlockComesToRest();
	void testFreezesAndWakes();
	void testCartRollsDownhill();
	void testWheelsRollFurtherThanASledgeSlides();
	void testHingeKeepsItsAxle();
	void testStableAndTopHeavy();
	void testHeldGoesWhereItsTold();
	void testPlayerPushes();
	void testSameEveryTime();
	void testPushedWhenFrozen();
	void testWalkHasInertia();
	void testJumpIsAHeight();
	void testStepsAndRides();
	void testCarriedFollowsOrDrags();
};

static TestPhysics g_test_instance;

void TestPhysics::runTests(IGameDef *gamedef)
{
	TEST(testBlockComesToRest);
	TEST(testFreezesAndWakes);
	TEST(testCartRollsDownhill);
	TEST(testWheelsRollFurtherThanASledgeSlides);
	TEST(testHingeKeepsItsAxle);
	TEST(testStableAndTopHeavy);
	TEST(testHeldGoesWhereItsTold);
	TEST(testPlayerPushes);
	TEST(testSameEveryTime);
	TEST(testPushedWhenFrozen);
	TEST(testWalkHasInertia);
	TEST(testJumpIsAHeight);
	TEST(testStepsAndRides);
	TEST(testCarriedFollowsOrDrags);
}

namespace {

constexpr float G = 9.81f;
constexpr float STONE = 2700, WOOD = 600;

// A flat floor everywhere: the nodes at y = -1, so its top is y = -0.5;
// each node in sixteenth slices along z, as shaped ground's boxes are.
bool flatGround(v3s16 block, std::vector<aabb3f> &boxes)
{
	if (block.Y != -1)
		return true;
	for (s16 x = 0; x < BLOCK; x++)
		for (s16 z = 0; z < BLOCK; z++)
			for (int s = 0; s < 16; s++)
				boxes.push_back(aabb3f(x - 0.5f, BLOCK - 1.5f, z - 0.5f + s / 16.f, x + 0.5f, BLOCK - 0.5f,
						z - 0.5f + (s + 1) / 16.f));
	return true;
}

World::Opts flat(v3f gravity = v3f(0, -G, 0))
{
	return World::Opts{ gravity, flatGround };
}

Body block(v3f centre, v3f size, float density, float friction = 0.6f)
{
	return Body{ { Box{ aabb3f(centre - size * 0.5f, centre + size * 0.5f), density, friction } }, std::nullopt };
}

Quat about(v3f axis, float angle)
{
	const float s = std::sin(angle / 2);
	return { axis.X * s, axis.Y * s, axis.Z * s, std::cos(angle / 2) };
}

v3f turned(const Quat &q, v3f v)
{
	const v3f u(q.x, q.y, q.z);
	return u * (2 * u.dotProduct(v)) + v * (q.w * q.w - u.dotProduct(u)) + u.crossProduct(v) * (2 * q.w);
}

// A cart: a wooden plate 2 long (x) and 1 wide, four wheels on hinges about
// z. Each axle's friction is rolling resistance: 0.02 of its share of the
// cart's weight (~470 kg) at the wheel's radius.
Assembly cart()
{
	Assembly a;
	a.bodies.push_back(block(v3f(0), v3f(2, 0.25f, 1), WOOD));
	for (float x : { -0.7f, 0.7f })
		for (float z : { -0.65f, 0.65f }) {
			const v3f c(x, -0.1f, z);
			Body w = block(c, v3f(0.6f, 0.6f, 0.2f), WOOD);
			w.round = Round{ c, v3f(0, 0, 1), 0.4f, 0.1f };
			a.hinges.push_back(Hinge{ 0, a.bodies.size(), c, v3f(0, 0, 1), 0.02f * 470 / 4 * G * 0.4f });
			a.bodies.push_back(w);
		}
	return a;
}

// The same plate on runners: no wheels, it slides.
Assembly sledge()
{
	Assembly a;
	a.bodies.push_back(block(v3f(0), v3f(2, 0.25f, 1), WOOD));
	a.bodies[0].boxes.push_back(Box{ aabb3f(-1, -0.5f, -0.6f, 1, -0.125f, -0.4f), WOOD, 0.6f });
	a.bodies[0].boxes.push_back(Box{ aabb3f(-1, -0.5f, 0.4f, 1, -0.125f, 0.6f), WOOD, 0.6f });
	return a;
}

void run(World &w, double seconds)
{
	for (double t = 0; t < seconds; t += STEP)
		w.step(STEP);
}

} // namespace

void TestPhysics::testBlockComesToRest()
{
	World w(flat());
	Assembly a;
	a.bodies.push_back(block(v3f(0), v3f(1), STONE));
	World::Id id = w.add(a, Pose{ v3d(3, 3, 3), {} });
	UASSERTEQ(float, w.mass(id), STONE);
	run(w, 4);
	const Pose p = w.pose(id);
	UASSERT(std::fabs(p.position.Y - 0) < 0.03); // resting on the floor's top
	UASSERT(std::fabs(p.position.X - 3) < 0.01 && std::fabs(p.position.Z - 3) < 0.01);
	UASSERT(w.asleep(id));
}

void TestPhysics::testFreezesAndWakes()
{
	World w(flat());
	Assembly a;
	a.bodies.push_back(block(v3f(0), v3f(1), STONE));
	World::Id id = w.add(a, Pose{ v3d(0, 2, 0), {} });
	run(w, 6);
	// at rest and no one near: frozen, and the ground round it let go
	UASSERT(w.tier(id) == Tier::Frozen);
	UASSERTEQ(size_t, w.groundBlocks(), 0);
	const Pose rest = w.pose(id);
	// a player comes near: it's Live again, where it was
	w.players({ Capsule{ v3d(2, -0.5, 0), 0.3f, 1.8f } });
	run(w, 0.1);
	UASSERT(w.tier(id) == Tier::Live);
	UASSERT(w.pose(id).position.getDistanceFrom(rest.position) < 0.01);
	UASSERT(w.groundBlocks() > 0);
	// they go: it freezes again
	w.players({});
	run(w, 2);
	UASSERT(w.tier(id) == Tier::Frozen);
}

void TestPhysics::testCartRollsDownhill()
{
	// a 10° slope: gravity turned 10° about z, downhill is +x
	const float a = 10 * core::DEGTORAD;
	World w(flat(v3f(G * std::sin(a), -G * std::cos(a), 0)));
	World::Id id = w.add(cart(), Pose{ v3d(0, 0.5, 0), {} });
	run(w, 1); // landed
	const double from = w.pose(id).position.X;
	// each wheel's turn, added up a step at a time (one reading wraps)
	auto angle = [](const Relative &r) { return 2 * std::atan2(r.rotation.z, r.rotation.w); };
	std::vector<float> last, turn(4, 0);
	for (const Relative &r : w.relatives(id))
		last.push_back(angle(r));
	for (double t = 0; t < 3; t += STEP) {
		w.step(STEP);
		const std::vector<Relative> rel = w.relatives(id);
		for (size_t i = 0; i < 4; i++) {
			float d = angle(rel[i]) - last[i];
			d -= std::round(d / (2 * core::PI)) * 2 * core::PI;
			turn[i] += d;
			last[i] = angle(rel[i]);
		}
	}
	UASSERT(w.velocity(id).X > 1.5f);
	const double went = w.pose(id).position.X - from;
	UASSERT(went > 2);
	// rolled, not slid: turned (clockwise from +z) as far as it went, at its radius
	for (float a : turn)
		UASSERT(std::fabs(-a * 0.4f - went) < went * 0.1);
}

void TestPhysics::testWheelsRollFurtherThanASledgeSlides()
{
	auto travel = [](const Assembly &a) {
		World w(flat());
		World::Id id = w.add(a, Pose{ v3d(0, 0.5, 0), {} }, v3f(3, 0, 0));
		run(w, 25);
		UASSERT(w.velocity(id).getLength() < 0.05f); // stopped
		return w.pose(id).position.X;
	};
	const double rolled = travel(cart()), slid = travel(sledge());
	UASSERT(rolled > 3);
	UASSERT(rolled > slid * 2);
}

void TestPhysics::testHingeKeepsItsAxle()
{
	const float a = 10 * core::DEGTORAD;
	World w(flat(v3f(G * std::sin(a), -G * std::cos(a), 0)));
	World::Id id = w.add(cart(), Pose{ v3d(0, 0.5, 0), {} });
	run(w, 3);
	const Assembly c = cart();
	const std::vector<Relative> rel = w.relatives(id);
	UASSERTEQ(size_t, rel.size(), 4);
	for (size_t i = 0; i < rel.size(); i++) {
		// where it's turned to, the wheel's middle is still its axle's
		const v3f centre = c.bodies[i + 1].round->centre;
		UASSERT((rel[i].offset + turned(rel[i].rotation, centre) - centre).getLength() < 0.02f);
		// and it's turned only about the axle
		UASSERT(std::fabs(rel[i].rotation.x) < 0.01f && std::fabs(rel[i].rotation.y) < 0.01f);
	}
}

void TestPhysics::testStableAndTopHeavy()
{
	auto upright = [](float base_density, float top_density) {
		World w(flat());
		Assembly a;
		// a narrow base and a wide top on a thin mast, tipped 12°
		a.bodies.push_back(block(v3f(0), v3f(0.6f, 0.5f, 0.6f), base_density));
		a.bodies[0].boxes.push_back(Box{ aabb3f(-0.1f, 0.25f, -0.1f, 0.1f, 2.25f, 0.1f), WOOD, 0.6f });
		a.bodies[0].boxes.push_back(Box{ aabb3f(-0.75f, 2.25f, -0.75f, 0.75f, 2.75f, 0.75f), top_density, 0.6f });
		World::Id id = w.add(a, Pose{ v3d(0, 0.1, 0), about(v3f(0, 0, 1), 12 * core::DEGTORAD) });
		run(w, 6);
		return turned(w.pose(id).rotation, v3f(0, 1, 0)).Y;
	};
	UASSERT(upright(STONE, WOOD / 10) > 0.98f); // stone at the bottom: it rights itself
	UASSERT(upright(WOOD / 10, STONE) < 0.5f);  // stone at the top: over it goes
}

void TestPhysics::testHeldGoesWhereItsTold()
{
	World w(flat());
	Assembly a;
	a.bodies.push_back(block(v3f(0), v3f(1), STONE));
	World::Id id = w.add(a, Pose{ v3d(0, 3, 0), {} });
	w.hold(id, true);
	w.moveTo(id, Pose{ v3d(2, 4, 1), about(v3f(0, 1, 0), 0.5f) });
	run(w, 0.5);
	UASSERT(w.pose(id).position.getDistanceFrom(v3d(2, 4, 1)) < 0.001);
	UASSERT(std::fabs(w.pose(id).rotation.y - about(v3f(0, 1, 0), 0.5f).y) < 0.001f);
	// let go, it falls from there
	w.hold(id, false);
	run(w, 3);
	UASSERT(std::fabs(w.pose(id).position.Y) < 0.05);
	UASSERT(std::fabs(w.pose(id).position.X - 2) < 0.05);
}

// A person (75 kg, pushing with 600 N) walks into a cart: it rolls, and
// slows them; into a 5 t block: it doesn't budge.
void TestPhysics::testPlayerPushes()
{
	const CharacterSpec person{ 0.3f, 1.8f, 75, 600, 50 };
	{
		World w(flat());
		World::Id id = w.add(cart(), Pose{ v3d(0, 0.5, 0), {} });
		run(w, 1);
		const double was = w.pose(id).position.X;
		World::CharId me = w.addCharacter(person, v3d(-2, -0.5, 0));
		for (double t = 0; t < 3; t += STEP) {
			const World::CharacterState c = w.character(me);
			w.want(me, v3f(3, c.velocity.Y - G * (float)STEP, 0));
			w.step(STEP);
		}
		UASSERT(w.pose(id).position.X > was + 1);
		UASSERT(w.character(me).foot.X < -2 + 3 * 3 - 1); // held up by it
	}
	{
		World w(flat());
		Assembly a;
		a.bodies.push_back(block(v3f(0), v3f(2, 1, 1), STONE));
		World::Id id = w.add(a, Pose{ v3d(0, 0, 0), {} });
		run(w, 1);
		World::CharId me = w.addCharacter(person, v3d(-2, -0.5, 0));
		for (double t = 0; t < 3; t += STEP) {
			const World::CharacterState c = w.character(me);
			w.want(me, v3f(3, c.velocity.Y - G * (float)STEP, 0));
			w.step(STEP);
		}
		UASSERT(std::fabs(w.pose(id).position.X) < 0.05);
		UASSERT(w.character(me).foot.X < -1.25); // against its side
	}
}

void TestPhysics::testSameEveryTime()
{
	auto once = [] {
		const float a = 10 * core::DEGTORAD;
		World w(flat(v3f(G * std::sin(a), -G * std::cos(a), 0)));
		World::Id id = w.add(cart(), Pose{ v3d(0, 0.5, 0), about(v3f(0, 1, 0), 0.3f) });
		World::Id other = w.add(sledge(), Pose{ v3d(4, 1, 0.5), {} });
		run(w, 4);
		return std::make_pair(w.pose(id), w.pose(other));
	};
	UASSERT(once() == once()); // bit for bit
}

// At rest and frozen, a push wakes it and it slides.
void TestPhysics::testPushedWhenFrozen()
{
	World w(flat());
	// two blocks side by side, as a machine of two is
	Assembly a;
	a.bodies.push_back(block(v3f(0), v3f(1), STONE));
	a.bodies[0].boxes.push_back(Box{ aabb3f(0.5f, -0.5f, -0.5f, 1.5f, 0.5f, 0.5f), STONE, 0.6f });
	World::Id id = w.add(a, Pose{ v3d(8, 6, 8), {} });
	run(w, 6);
	UASSERT(w.tier(id) == Tier::Frozen);
	w.impulse(id, v3f(STONE * 2 * 2, 0, 0)); // 2 nodes/s
	run(w, 2);
	// sliding against friction 0.6 from 2 nodes/s: 2² / (2 · 0.6 · g) ≈ 0.34
	UASSERT(w.pose(id).position.X > 8.25);
}

namespace {

// Walking for `seconds` asked so, on the ground (or not), from `v`.
v3f walked(const Gait &g, const Intent &i, v3f v, double seconds, bool ground = true, v3f under = v3f(0))
{
	for (double t = 0; t < seconds - 1e-9; t += STEP)
		v = walk(g, i, Footing{ v, ground, under }, (float)STEP);
	return v;
}

} // namespace

void TestPhysics::testWalkHasInertia()
{
	const Gait g;
	const Intent ahead{ 1, 0, 0, false }; // yaw 0: forward is +z
	// up to walking speed in about a quarter of a second, not at once
	UASSERT(walked(g, ahead, v3f(0), STEP).Z < 1);
	UASSERT(walked(g, ahead, v3f(0), 0.15).Z < g.walk * 0.8f);
	UASSERT(std::fabs(walked(g, ahead, v3f(0), 0.3).Z - g.walk) < 1e-3f);
	// running: past walking speed slowly
	Gait run = g;
	run.run = 1.6f;
	const v3f walking = walked(g, ahead, v3f(0), 0.3);
	UASSERT(walked(run, ahead, walking, 0.2).Z < g.walk * 1.3f);
	UASSERT(std::fabs(walked(run, ahead, walking, 0.6).Z - g.walk * 1.6f) < 1e-3f);
	// let go: stops sooner than it started
	UASSERT(walked(g, Intent{}, v3f(0, 0, g.walk), 0.15).getLength() < 1e-3f);
	// turned right at a run: a curve, still going forward a while
	const v3f turning = walked(run, Intent{ 0, 1, 0, false }, v3f(0, 0, g.walk * 1.6f), 0.1);
	UASSERT(turning.Z > 2 && turning.X > 0.5f);
	// yaw: 90° faces −x
	UASSERT(walked(g, Intent{ 1, 0, core::PI / 2, false }, v3f(0), 0.5).X < -g.walk + 1e-3f);
	// in the air, it barely steers; it falls
	const v3f air = walked(g, ahead, v3f(0), 0.5, false);
	UASSERT(air.Z < 2 && air.Y < -4);
	// on a moving cart, standing still is moving with it
	const v3f under(2, 0, 0);
	UASSERT(walked(g, Intent{}, under, 0.5, true, under).getDistanceFrom(under) < 1e-3f);
}

void TestPhysics::testJumpIsAHeight()
{
	for (float gravity : { 9.81f, 1.62f }) { // here and on a moon: the same height
		Gait g;
		g.gravity = gravity;
		v3f v = walk(g, Intent{ 0, 0, 0, true }, Footing{ v3f(0), true }, (float)STEP);
		double y = 0, top = 0;
		for (int i = 0; i < 2000 && (i == 0 || y > 0); i++) {
			y += v.Y * STEP;
			top = std::max(top, y);
			v = walk(g, Intent{}, Footing{ v, false }, (float)STEP);
		}
		UASSERT(std::fabs(top - g.jump) < 0.05);
	}
}

void TestPhysics::testStepsAndRides()
{
	const CharacterSpec person{ 0.3f, 1.8f, 75, 600, 50 };
	const Gait g;
	auto go = [&](World &w, World::CharId me, const Intent &i, double seconds) {
		for (double t = 0; t < seconds; t += STEP) {
			const World::CharacterState c = w.character(me);
			w.want(me, walk(g, i, Footing{ c.velocity, c.on_ground, c.ground_velocity }, (float)STEP));
			w.step(STEP);
		}
	};
	// a step half a node high it walks up; a whole node it doesn't
	for (float rise : { 0.5f, 1.0f }) {
		World w(flat());
		Assembly a;
		a.bodies.push_back(block(v3f(0), v3f(12, rise, 4), STONE)); // from x 1 on, longer than the walk
		World::Id id = w.add(a, Pose{ v3d(7, -0.5 + rise / 2, 0), {} });
		w.hold(id, true); // a fixed step
		World::CharId me = w.addCharacter(person, v3d(0, -0.5, 0));
		go(w, me, Intent{ 0, 1, 0, false }, 2); // right is +x at yaw 0
		const World::CharacterState c = w.character(me);
		if (rise < 0.6f) {
			UASSERT(c.foot.Y > -0.5 + rise - 0.05 && c.foot.X > 1.5);
		} else {
			UASSERT(c.foot.Y < -0.4 && c.foot.X < 1.0);
		}
	}
	// standing on a platform moving at 2 nodes/s, it's carried along
	World w(flat());
	Assembly a;
	a.bodies.push_back(block(v3f(0), v3f(4, 0.5f, 4), STONE));
	World::Id id = w.add(a, Pose{ v3d(0, 0.25, 0), {} });
	w.hold(id, true);
	World::CharId me = w.addCharacter(person, v3d(0, 0.5, 0));
	go(w, me, Intent{}, 0.5); // settles on it
	const double from = w.character(me).foot.X;
	for (double t = 0; t < 1; t += STEP) {
		w.moveTo(id, Pose{ v3d(2 * (t + STEP), 0.25, 0), {} });
		go(w, me, Intent{}, STEP);
	}
	UASSERT(std::fabs(w.character(me).foot.X - from - 2) < 0.15);
	UASSERT(w.character(me).on_ground);
}

// Carried by someone with 2000 N: a wooden block follows the hands; a
// stone one more than they can hold up sags to the ground and drags.
void TestPhysics::testCarriedFollowsOrDrags()
{
	for (float density : { WOOD, STONE }) {
		World w(flat());
		Assembly a;
		a.bodies.push_back(block(v3f(0), v3f(0.5f), density));
		World::Id id = w.add(a, Pose{ v3d(0, 0, 0), {} });
		run(w, 0.5);
		w.carry(id, 2000.f);
		for (double t = 0; t < 2; t += STEP) {
			w.pullTo(id, Pose{ v3d(t, 1.5, 0), {} }); // carried along at a node a second, up high
			w.step(STEP);
		}
		const v3d p = w.pose(id).position;
		if (density == WOOD) { // 75 kg: in the hands
			UASSERT(p.getDistanceFrom(v3d(2, 1.5, 0)) < 0.15);
		} else { // 340 kg: on the ground
			UASSERT(p.Y < 0);
		}
		w.carry(id, std::nullopt);
		run(w, 3);
		UASSERT(std::fabs(w.pose(id).position.Y - (-0.25)) < 0.05); // let go, it falls
	}
}
