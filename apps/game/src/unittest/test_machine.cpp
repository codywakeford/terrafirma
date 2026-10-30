// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "test.h"

#include "dummygamedef.h"
#include "exceptions.h"
#include "nodedef.h"
#include "piece/frame.h"
#include "piece/header.h"
#include "piece/machine.h"
#include "sculpt/sculpt.h"
#include "workshop/motion.h"
#include <cmath>
#include <set>

using namespace piece;

class TestMachine : public TestBase {
public:
	TestMachine() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestMachine"; }

	void runTests(IGameDef *gamedef);

	void testTurns();
	void testWrittenAndReadBack();
	void testGrownOffAFace();
	void testJoinedFaceToFace();
	void testFaceToward();
	void testSockets();
	void testRoomMade();
	void testHingeThroughTheMiddle();
	void testRigidGroups();
	void testCompiledForPhysics();
	void testEntityTurns();
};

static TestMachine g_test_instance;

void TestMachine::runTests(IGameDef *gamedef)
{
	TEST(testTurns);
	TEST(testWrittenAndReadBack);
	TEST(testGrownOffAFace);
	TEST(testJoinedFaceToFace);
	TEST(testFaceToward);
	TEST(testSockets);
	TEST(testRoomMade);
	TEST(testHingeThroughTheMiddle);
	TEST(testRigidGroups);
	TEST(testCompiledForPhysics);
	TEST(testEntityTurns);
}

namespace {

struct Game
{
	DummyGameDef gamedef;
	content_t planks;

	Game()
	{
		ContentFeatures f;
		f.name = "test:planks";
		f.walkable = true;
		f.drawtype = NDT_NORMAL;
		planks = gamedef.getWritableNodeDefManager()->set(f.name, std::move(f));
	}

	Machine::Part block(v3s32 offset = v3s32(0), u8 turn = 0)
	{
		Object o(&gamedef);
		o.design().map().setNode({ 0, 0, 0 }, MapNode(planks));
		o.build();
		return { std::move(o), Pose{ offset, turn } };
	}
};

// Where a part's point comes to in the machine: as its frame (BS) takes it.
v3f byFrame(const Pose &pose, v3f sixteenths)
{
	v3f p = (sixteenths / 16.f - v3f(0.5f)) * BS;
	pose.matrix().transformVect(p);
	return (p / BS + v3f(0.5f)) * 16.f;
}

} // namespace

// 24 turns, each a turn (not a mirror), none twice; composed and undone
// as they should be; a pose's frame takes a point where its sums do.
void TestMachine::testTurns()
{
	std::set<std::tuple<int, int, int, int, int, int>> seen;
	for (u8 t = 0; t < TURN_COUNT; t++) {
		const Turn &q = turn(t);
		const v3f x = v3f::from(q.axes[0]), y = v3f::from(q.axes[1]), z = v3f::from(q.axes[2]);
		UASSERT(std::abs(x.dotProduct(y.crossProduct(z)) - 1) < 1e-6f);
		seen.insert({ q.axes[0].X, q.axes[0].Y, q.axes[0].Z, q.axes[1].X, q.axes[1].Y, q.axes[1].Z });
		const Pose pose{ v3s32(5, -7, 33), t };
		const v3f p(3.5f, 17, -2.25f);
		UASSERT(pose.unapply(pose.apply(p)).getDistanceFrom(p) < 1e-4f);
		UASSERT(byFrame(pose, p).getDistanceFrom(pose.apply(p)) < 1e-3f);
		for (u8 u = 0; u < TURN_COUNT; u++) {
			const v3s32 v(1, 2, 3);
			UASSERT(turn(compose(t, u)).apply(v) == turn(u).apply(turn(t).apply(v)));
		}
	}
	UASSERTEQ(size_t, seen.size(), size_t(TURN_COUNT));
	UASSERT(turn(0).apply(v3s32(1, 2, 3)) == v3s32(1, 2, 3));
	const u8 up = turnTaking(v3s16(0, 0, 1), v3s16(0, 1, 0), 0);
	UASSERT(turn(up).apply(v3s32(0, 0, 1)) == v3s32(0, 1, 0));
	EXCEPTION_CHECK(SerializationError, turn(24));
}

// A machine's parts, where they sit, how they were made, and its joints
// survive its text; its boxes are its parts' set where they sit. An
// object's text reads as a machine of one part, of its id.
void TestMachine::testWrittenAndReadBack()
{
	Game g;
	Machine m(&g.gamedef);
	m.add(g.block());
	Machine::Part &second = m.add(g.block(v3s32(16, 0, 0), 5));
	m.links().push_back({ m.parts()[0].object.id(), second.object.id(), Joint::Hinge, v3f(16, 8, 8), v3s16(1, 0, 0) });

	Machine back(&g.gamedef);
	back.read(m.write());
	UASSERT(back.id() == m.id() && back.write() == m.write());
	UASSERTEQ(size_t, back.parts().size(), size_t(2));
	UASSERT(back.parts()[1].pose.turn == 5 && back.parts()[1].pose.offset == v3s32(16, 0, 0));
	UASSERT(back.links().size() == 1 && back.links()[0].joint == Joint::Hinge);

	const Header h = header(m.write());
	UASSERT(h.id == m.id());
	UASSERT(h.designed->MinEdge.getDistanceFrom(v3f(-0.5f)) < 1e-4f);
	UASSERT(h.designed->MaxEdge.getDistanceFrom(v3f(1.5f, 0.5f, 0.5f)) < 1e-4f);

	Object o(&g.gamedef);
	o.design().map().setNode({ 0, 0, 0 }, MapNode(g.planks));
	Machine one(&g.gamedef);
	one.read(o.write());
	UASSERT(one.id() == o.id() && one.parts().size() == 1 && header(o.write()).id == o.id());

	m.renew();
	UASSERT(m.id() != back.id() && m.links()[0].b == m.parts()[1].object.id()
			&& m.parts()[1].object.id() != back.parts()[1].object.id());
	Machine broken(&g.gamedef);
	std::string text = back.write();
	m.links()[0].a = 999;
	EXCEPTION_CHECK(SerializationError, broken.read(m.write()));
}

// A cylinder drawn out of a block's +x face as a part of its own (a
// wheel): its cells where the stroke put them, on a grid of its own, and
// hinged to the block at the face, about +x; mirrored across the block's
// middle, its pair's face is the −x one, facing out. A box grown welded.
void TestMachine::testGrownOffAFace()
{
	Game g;
	Machine m(&g.gamedef);
	const u64 root = m.add(g.block()).object.id();
	const shape::Sixteenth at{ v3s16(0, 0, 0), v3s16(15, 7, 7) };
	const Machine::Face east{ root, at, v3s16(1, 0, 0) };
	UASSERT(m.point(east).getDistanceFrom(v3f(16, 7.5f, 7.5f)) < 1e-4f);

	const sculpt::Stroke wheel{ sculpt::Tool::Cylinder, 4, at, v3s16(1, 0, 0), sculpt::Act::Add, std::nullopt,
		shape::Sixteenth{ v3s16(0, 1, 1), v3s16(15, 10, 10) } };
	UASSERT(wheel.valid());
	const std::vector<v3s32> cells = wheel.cells();
	const Machine::Part &grown = m.grow(east, cells, g.planks, Joint::Hinge);
	// every cell where the stroke put it
	size_t n = 0;
	grown.object.design().each([&](v3s16 p, MapNode, const shape::Shape *s) {
		for (s16 x = 0; x < 16; x++)
			for (s16 y = 0; y < 16; y++)
				for (s16 z = 0; z < 16; z++)
					if (!s || s->has({ x, y, z })) {
						const v3f c = grown.pose.apply(v3f(p.X * 16 + x, p.Y * 16 + y, p.Z * 16 + z));
						UASSERT(std::find(cells.begin(), cells.end(), v3s32(c.X, c.Y, c.Z)) != cells.end());
						n++;
					}
	});
	UASSERTEQ(size_t, n, cells.size());
	UASSERT(m.links().back().joint == Joint::Hinge && m.links().back().axis == v3s16(1, 0, 0)
			&& m.links().back().a == root && m.links().back().b == grown.object.id());
	// out of the face, not into the block
	for (const v3s32 &c : cells)
		UASSERT(c.X >= 16 && c.X < 20);

	const Machine::Face west = m.mirrored(east, 0, 16); // the block's middle: x 8
	UASSERT(west.part == root && west.normal == v3s16(-1, 0, 0) && west.at.cell == v3s16(0, 7, 7));
	UASSERT(m.point(west).getDistanceFrom(v3f(0, 7.5f, 7.5f)) < 1e-4f);

	const sculpt::Stroke box{ sculpt::Tool::Box, 16, { v3s16(0, 0, 0), v3s16(3, 15, 12) }, v3s16(0, 1, 0),
		sculpt::Act::Add, std::nullopt };
	const Machine::Part &up = m.grow({ root, box.at, box.normal }, box.cells(), g.planks, Joint::Fixed);
	UASSERT(boxOf(up.object.design().write()));
	UASSERT(up.pose.offset == v3s32(0, 16, 0) && m.links().back().joint == Joint::Fixed);
	EXCEPTION_CHECK(BaseException, m.grow(east, {}, g.planks, Joint::Fixed));
}

// Another machine joined by its −y face to this one's +y face: set on top,
// its faces flat against ours, one machine, joined there.
void TestMachine::testJoinedFaceToFace()
{
	Game g;
	Machine a(&g.gamedef), b(&g.gamedef);
	const u64 pa = a.add(g.block()).object.id();
	const u64 pb = b.add(g.block(v3s32(0), 9)).object.id(); // b's part is turned
	const Machine::Face up{ pa, { v3s16(0, 0, 0), v3s16(7, 15, 7) }, v3s16(0, 1, 0) };
	// the face of b's part that faces its machine's −y, whichever it is
	v3s16 local;
	const Turn &t9 = turn(9);
	for (int ax = 0; ax < 3; ax++)
		local[ax] = s16(-t9.axes[ax].Y);
	const v3s16 cell(local.X > 0 ? 15 : local.X < 0 ? 0 : 7, local.Y > 0 ? 15 : local.Y < 0 ? 0 : 7,
			local.Z > 0 ? 15 : local.Z < 0 ? 0 : 7);
	const Machine::Face down{ pb, { v3s16(0, 0, 0), cell }, local };
	UASSERT(b.normal(down) == v3s16(0, -1, 0));
	a.join(up, std::move(b), down, Joint::Fixed);
	UASSERT(a.parts().size() == 2);
	// the faces clicked meet, middle to middle, facing each other: b's
	// block is on top of a's, its foot on a's top
	UASSERT(a.point(down).getDistanceFrom(a.point(up)) < 1e-3f);
	UASSERT(a.normal(down) == v3s16(0, -1, 0));
	const Machine::Part *on = a.part(pb);
	f32 low = 1e9f;
	for (int c = 0; c < 8; c++)
		low = std::min(low, on->pose.apply(v3f(c & 1 ? 16 : 0, c & 2 ? 16 : 0, c & 4 ? 16 : 0)).Y);
	UASSERT(std::abs(low - 16) < 1e-3f);
	UASSERT(a.links().size() == 1 && a.links()[0].a == pa && a.links()[0].b == pb);
}

// The face it's joined by, carried onto another: of the node reaching
// furthest that way, in its own part's terms however that part's turned.
void TestMachine::testFaceToward()
{
	Game g;
	Machine m(&g.gamedef);
	const u64 a = m.add(g.block()).object.id();
	const u64 b = m.add(g.block(v3s32(32, 0, 16), 9)).object.id(); // turned, out along +x
	const std::optional<Machine::Face> east = m.faceToward(v3s16(1, 0, 0));
	UASSERT(east && east->part == b && m.normal(*east) == v3s16(1, 0, 0));
	UASSERT(m.point(*east).X >= 32 - 0.01f); // on the far side of b
	const std::optional<Machine::Face> west = m.faceToward(v3s16(-1, 0, 0));
	UASSERT(west && west->part == a && m.normal(*west) == v3s16(-1, 0, 0));
	UASSERT(std::abs(m.point(*west).X) < 0.01f);
	UASSERT(!Machine(&g.gamedef).faceToward(v3s16(0, 1, 0)));
}

// A socket on a face: written and read back; something carried onto it is
// set flat against its face, middles met, joined as it says (a hinge about
// its axis), and it's taken up. A part taken off takes its sockets.
void TestMachine::testSockets()
{
	Game g;
	Machine a(&g.gamedef);
	const u64 root = a.add(g.block()).object.id();
	const Machine::Face east{ root, { v3s16(0, 0, 0), v3s16(15, 7, 7) }, v3s16(1, 0, 0) };
	const u64 hinge = a.socket(east, Joint::Hinge).id;
	UASSERT(a.sockets().size() == 1 && a.sockets()[0].axis == v3s16(1, 0, 0));
	Machine back(&g.gamedef);
	back.read(a.write());
	UASSERT(back.sockets().size() == 1 && back.sockets()[0].id == hinge && back.sockets()[0].joint == Joint::Hinge);

	Machine wheel(&g.gamedef);
	const u64 w = wheel.add(g.block()).object.id();
	const Machine::Face west{ w, { v3s16(0, 0, 0), v3s16(0, 7, 7) }, v3s16(-1, 0, 0) };
	a.plug(hinge, std::move(wheel), west);
	UASSERT(a.sockets().empty() && a.parts().size() == 2);
	UASSERT(a.links().back().joint == Joint::Hinge && a.links().back().axis == v3s16(1, 0, 0)
			&& a.links().back().a == root && a.links().back().b == w);
	UASSERT(a.part(w)->pose.offset == v3s32(16, 0, 0)); // the next node out, face to face
	EXCEPTION_CHECK(BaseException, a.plug(hinge, Machine(&g.gamedef), west));

	a.socket({ w, { v3s16(0, 0, 0), v3s16(15, 7, 7) }, v3s16(1, 0, 0) }, Joint::Fixed);
	a.take(w);
	UASSERT(a.sockets().empty());
}

// Adding past a part's grid's least side, its grid's moved to make room:
// what's on it moves along it and the part the other way, so it's where it
// was; however the part's turned. What won't fit a grid throws.
void TestMachine::testRoomMade()
{
	Game g;
	for (const u8 t : { u8(0), u8(9) }) {
		Machine m(&g.gamedef);
		const u64 p = m.add(g.block(v3s32(32, 0, 0), t)).object.id();
		const v3f was = m.part(p)->pose.apply(v3f(8, 8, 8));
		const v3s16 by = m.makeRoom(p, v3s32(-2, 0, 0), v3s32(-1, 0, 0));
		UASSERT(by == v3s16(2, 0, 0));
		const Machine::Part &part = *m.part(p);
		UASSERT(part.object.design().node({ 2, 0, 0 }).getContent() == g.planks);
		UASSERT(part.object.design().node({ 0, 0, 0 }).getContent() == CONTENT_AIR);
		UASSERT(part.pose.apply(v3f(2 * 16 + 8, 8, 8)).getDistanceFrom(was) < 1e-3f);
		UASSERT(m.makeRoom(p, v3s32(0, 0, 0), v3s32(3, 0, 0)) == v3s16(0, 0, 0)); // room enough
		EXCEPTION_CHECK(BaseException, m.makeRoom(p, v3s32(-14, 0, 0), v3s32(-13, 0, 0)));
	}
}

namespace {

// A wheel drawn on a block's east face by dragging from (15, 7, 7) to the
// next node's (1, 10, 10): 20 sixteenths across, 4 deep.
const sculpt::Stroke WHEEL{ sculpt::Tool::Cylinder, 4, { v3s16(0, 0, 0), v3s16(15, 7, 7) }, v3s16(1, 0, 0),
	sculpt::Act::Add, std::nullopt, shape::Sixteenth{ v3s16(0, 1, 1), v3s16(15, 10, 10) } };

} // namespace

// A hinged part turns about what's drawn's middle, not where the drag began.
void TestMachine::testHingeThroughTheMiddle()
{
	Game g;
	Machine m(&g.gamedef);
	const u64 root = m.add(g.block()).object.id();
	const std::vector<v3s32> cells = WHEEL.cells();
	m.grow({ root, WHEEL.at, WHEEL.normal }, cells, g.planks, Joint::Hinge);
	v3f lo(1e9f), hi(-1e9f);
	for (const v3s32 &c : cells)
		for (int a = 0; a < 3; a++) {
			lo[a] = std::min(lo[a], (f32)c[a]);
			hi[a] = std::max(hi[a], (f32)c[a] + 1);
		}
	const v3f anchor = m.links().back().anchor;
	UASSERT(std::fabs(anchor.X - 16) < 1e-4f); // on the face
	UASSERT(std::fabs(anchor.Y - (lo.Y + hi.Y) / 2) < 1e-4f && std::fabs(anchor.Z - (lo.Z + hi.Z) / 2) < 1e-4f);
	// welded, where it began is kept (it's where it's stuck on)
	const sculpt::Stroke plate{ sculpt::Tool::Box, 4, WHEEL.at, WHEEL.normal, sculpt::Act::Add, std::nullopt, WHEEL.to };
	m.grow({ root, plate.at, plate.normal }, plate.cells(), g.planks, Joint::Fixed);
	UASSERT(m.links().back().anchor.getDistanceFrom(m.point({ root, WHEEL.at, WHEEL.normal })) < 1e-4f);
}

void TestMachine::testRigidGroups()
{
	Game g;
	Machine m(&g.gamedef);
	const u64 root = m.add(g.block()).object.id();
	const sculpt::Stroke box{ sculpt::Tool::Box, 16, { v3s16(0, 0, 0), v3s16(3, 15, 12) }, v3s16(0, 1, 0),
		sculpt::Act::Add, std::nullopt };
	const u64 up = m.grow({ root, box.at, box.normal }, box.cells(), g.planks, Joint::Fixed).object.id();
	const u64 wheel = m.grow({ root, WHEEL.at, WHEEL.normal }, WHEEL.cells(), g.planks, Joint::Hinge).object.id();
	const u64 loose = m.add(g.block(v3s32(-64, 0, 0))).object.id(); // no joint: it's welded to the first
	const std::vector<std::vector<u64>> groups = m.rigid();
	UASSERTEQ(size_t, groups.size(), 2);
	UASSERT(groups[0] == std::vector<u64>({ root, up, loose }));
	UASSERT(groups[1] == std::vector<u64>({ wheel }));
	// a hinge made a weld: one group
	m.relink(root, wheel, Joint::Fixed, std::nullopt);
	UASSERTEQ(size_t, m.rigid().size(), 1);
}

void TestMachine::testCompiledForPhysics()
{
	Game g;
	Machine m(&g.gamedef);
	const u64 root = m.add(g.block()).object.id();
	m.grow({ root, WHEEL.at, WHEEL.normal }, WHEEL.cells(), g.planks, Joint::Hinge);
	workshop::Materials materials;
	EXCEPTION_CHECK(std::runtime_error, workshop::compile(m, materials, &g.gamedef)); // planks weigh nothing yet
	materials.set(g.planks, { 600, 0.6f });
	const physics::Assembly compiled = workshop::compile(m, materials, &g.gamedef);
	UASSERTEQ(size_t, compiled.bodies.size(), 2);
	UASSERTEQ(size_t, compiled.hinges.size(), 1);
	// the block: a node, from its centre, 600 kg
	float kg = 0;
	for (const physics::Box &b : compiled.bodies[0].boxes)
		kg += b.box.getVolume() * b.density;
	UASSERT(std::fabs(kg - 600) < 0.01f);
	UASSERT(compiled.bodies[0].boxes[0].box.MinEdge.getDistanceFrom(v3f(-0.5f)) < 1e-5f);
	UASSERT(!compiled.bodies[0].round);
	// the wheel rolls as a cylinder 10 sixteenths round, 4 thick, on its axle
	const std::optional<physics::Round> &r = compiled.bodies[1].round;
	UASSERT(r.has_value());
	UASSERT(std::fabs(r->radius - 10 / 16.f) < 1.5f / 16);
	UASSERT(std::fabs(r->half_width - 2 / 16.f) < 1e-4f);
	UASSERT(r->axis == v3f(1, 0, 0));
	UASSERT(std::fabs(r->centre.X - (18 / 16.f - 0.5f)) < 1e-4f);
	const physics::Hinge &h = compiled.hinges[0];
	UASSERT(h.a == 0 && h.b == 1 && h.axis == v3f(1, 0, 0));
	UASSERT(std::fabs(h.anchor.Y - r->centre.Y) < 1e-4f && std::fabs(h.anchor.Z - r->centre.Z) < 1e-4f);
	UASSERT(h.friction > 0);
	// a square plate on a hinge isn't round: it'd roll lumpy, as it is
	Machine sq(&g.gamedef);
	const u64 sr = sq.add(g.block()).object.id();
	const sculpt::Stroke plate{ sculpt::Tool::Box, 4, WHEEL.at, WHEEL.normal, sculpt::Act::Add, std::nullopt, WHEEL.to };
	sq.grow({ sr, plate.at, plate.normal }, plate.cells(), g.planks, Joint::Hinge);
	UASSERT(!workshop::compile(sq, materials, &g.gamedef).bodies[1].round);
}

// An entity's turn (degrees, as piece::frame draws it) as physics' quaternion and back.
void TestMachine::testEntityTurns()
{
	const v3f turns[] = { v3f(0), v3f(0, 90, 0), v3f(30, 0, 0), v3f(0, 0, 45), v3f(20, 135, 310), v3f(80, 200, 10) };
	const v3f probes[] = { v3f(1, 0, 0), v3f(0, 1, 0), v3f(0, 0, 1), v3f(0.3f, -0.7f, 0.2f) };
	auto by = [](const physics::Quat &q, v3f v) {
		const v3f u(q.x, q.y, q.z);
		return u * (2 * u.dotProduct(v)) + v * (q.w * q.w - u.dotProduct(u)) + u.crossProduct(v) * (2 * q.w);
	};
	for (const v3f &r : turns) {
		const physics::Quat q = workshop::quatOf(r);
		const v3f back = workshop::rotationOf(q);
		for (v3f v : probes) {
			v3f drawn = v, again = v;
			piece::frame(v3f(0), r).transformVect(drawn);
			piece::frame(v3f(0), back).transformVect(again);
			UASSERT(by(q, v).getDistanceFrom(drawn) < 1e-4f);
			UASSERT(again.getDistanceFrom(drawn) < 1e-4f);
		}
	}
}
