// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "test.h"

#include "collision.h"
#include "dummymap.h"
#include "environment.h"
#include "gamedef.h"
#include "nodedef.h"
#include "mapblock.h"
#include "planet/planet.h"
#include "raycast.h"
#include "voxelalgorithms.h"

/*
	The engine on a planet (planning/sprint-26-planet.md, phase 2): a real
	Map holding a small planet's faces, and what the engine does at their
	edges. Each system that learns to cross an edge gets its test here.
*/
class TestPlanetMap : public TestBase {
public:
	TestPlanetMap() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestPlanetMap"; }

	void runTests(IGameDef *gamedef);

	void testNextFlat(IGameDef *gamedef);
	void testNextAcrossEdges(IGameDef *gamedef);
	void testNodesAcrossEdges(IGameDef *gamedef);
	void testSeenAcrossEdges(IGameDef *gamedef);
	void testWalkOverEdges(IGameDef *gamedef);
	void testPointAcrossEdges(IGameDef *gamedef);
	void testCornerNeighbourhood(IGameDef *gamedef);
	void testLightAcrossEdges(IGameDef *gamedef);
};

static TestPlanetMap g_test_instance;

void TestPlanetMap::runTests(IGameDef *gamedef)
{
	TEST(testNextFlat, gamedef);
	TEST(testNextAcrossEdges, gamedef);
	TEST(testNodesAcrossEdges, gamedef);
	TEST(testSeenAcrossEdges, gamedef);
	TEST(testWalkOverEdges, gamedef);
	TEST(testPointAcrossEdges, gamedef);
	TEST(testCornerNeighbourhood, gamedef);
	TEST(testLightAcrossEdges, gamedef);
}

// A planet small enough to hold whole: faces 128 nodes across.
static const planet::Planet SMALL(64);
static const v3s16 DIRS[4] = { { 1, 0, 0 }, { 0, 0, 1 }, { -1, 0, 0 }, { 0, 0, -1 } };

// All of the small planet's net, a few blocks high, in a map.
static std::unique_ptr<DummyMap> planetMap(IGameDef *gamedef)
{
	const planet::Planet::Square low = SMALL.square(0), high = SMALL.square(5);
	auto map = std::make_unique<DummyMap>(gamedef,
		getNodeBlockPos(v3s16(low.x, 0, low.z)),
		getNodeBlockPos(v3s16(high.x + SMALL.width() - 1, 31, high.z + SMALL.width() - 1)));
	map->setPlanet(&SMALL);
	return map;
}

// Every node along a face's side `d`: the last row before the edge.
template <typename F>
static void alongSide(int face, int d, F f)
{
	const planet::Planet::Square s = SMALL.square(face);
	const s16 w = SMALL.width();
	for (s16 i = 0; i < w; i++) {
		s16 x = s.x + i, z = s.z + i;
		if (DIRS[d].X != 0)
			x = DIRS[d].X > 0 ? s.x + w - 1 : s.x;
		else
			z = DIRS[d].Z > 0 ? s.z + w - 1 : s.z;
		f(x, z);
	}
}

void TestPlanetMap::testNextFlat(IGameDef *gamedef)
{
	DummyMap map(gamedef, v3s16(0, 0, 0), v3s16(0, 0, 0));
	const Map::Next n = map.next(v3s16(3, 4, 5), v3s16(0, 0, -1));
	UASSERT(n.pos == v3s16(3, 4, 4) && n.turn == 0);
	EXCEPTION_CHECK(BaseException, map.next(v3s16(), v3s16(1, 1, 0)));
}

// Off every side of every face, the map's next node is the planet's step;
// up and down never leave the column.
void TestPlanetMap::testNextAcrossEdges(IGameDef *gamedef)
{
	auto map = planetMap(gamedef);
	for (int f = 0; f < 6; f++)
	for (int d = 0; d < 4; d++)
		alongSide(f, d, [&](s16 x, s16 z) {
			const Map::Next n = map->next(v3s16(x, 7, z), DIRS[d]);
			const planet::Planet::Step s = SMALL.step(x, z, DIRS[d].X, DIRS[d].Z);
			UASSERT(n.pos == v3s16(s.x, 7, s.z) && n.turn == s.turn);
			UASSERT(map->next(v3s16(x, 7, z), v3s16(0, 1, 0)).pos == v3s16(x, 8, z));
		});
}

// A node set across an edge is the one read across it: the faces' blocks
// are where the map keeps them.
void TestPlanetMap::testNodesAcrossEdges(IGameDef *gamedef)
{
	auto map = planetMap(gamedef);
	content_t marker = 1;
	for (int f = 0; f < 6; f++)
	for (int d = 0; d < 4; d++)
		alongSide(f, d, [&](s16 x, s16 z) {
			const v3s16 there = map->next(v3s16(x, 5, z), DIRS[d]).pos;
			map->setNode(there, MapNode(marker));
			UASSERTEQ(content_t, map->getNode(map->next(v3s16(x, 5, z), DIRS[d]).pos).getContent(), marker);
			marker = marker % 100 + 1;
		});
}

// facedir 0–3: the way a node faces (builtin's core.facedir_to_dir)
static const v3s16 FACING[4] = { { 0, 0, 1 }, { 1, 0, 0 }, { 0, 0, -1 }, { -1, 0, 0 } };
static u8 facedir(v3s16 d)
{
	for (u8 i = 0; i < 4; i++)
		if (FACING[i] == d)
			return i;
	throw BaseException("facedir: not a way along the ground");
}

// Seen from a face, past its edge is the neighbour face's node, turned into
// this face's grid: a stair facing on across the edge there faces on across
// it here. On the face, and seen from a gap, it's the node itself.
void TestPlanetMap::testSeenAcrossEdges(IGameDef *gamedef)
{
	auto map = planetMap(gamedef);
	for (int f = 0; f < 6; f++)
	for (int d = 0; d < 4; d++)
		alongSide(f, d, [&](s16 x, s16 z) {
			const v3s16 here(x, 5, z), past = here + DIRS[d];
			const Map::Next across = map->next(here, DIRS[d]);
			const auto on = planet::Planet::turned(DIRS[d].X, DIRS[d].Z, across.turn);
			map->setNode(across.pos, MapNode(t_CONTENT_FACEDIR, 0, facedir(v3s16(on[0], 0, on[1]))));
			const Map::Next seen = map->see(here, past);
			UASSERT(seen.pos == across.pos && seen.turn == across.turn);
			const MapNode n = map->getNodeSeen(here, past);
			UASSERTEQ(content_t, n.getContent(), t_CONTENT_FACEDIR);
			UASSERT(FACING[n.getParam2()] == DIRS[d]);
			// on the face: itself
			UASSERT(map->see(here, here).pos == here);
		});
	// seen from a gap: just the map
	const planet::Planet::Square s = SMALL.square(0);
	const v3s16 gap(s.x + SMALL.width(), 5, s.z + 3);
	UASSERT(map->see(gap, gap + v3s16(1, 0, 0)).pos == gap + v3s16(1, 0, 0));
}

namespace {

// The small planet as a world: air, a stone floor on every face at y = 0,
// and brick in the gaps (the game's seam: solid).
class PlanetEnvironment : public Environment
{
public:
	explicit PlanetEnvironment(IGameDef *gamedef) : Environment(gamedef), m_map(planetMap(gamedef))
	{
		const planet::Planet::Square low = SMALL.square(0), high = SMALL.square(5);
		m_map->fill(getNodeBlockPos(v3s16(low.x, 0, low.z)),
			getNodeBlockPos(v3s16(high.x + SMALL.width() - 1, 31, high.z + SMALL.width() - 1)), MapNode(CONTENT_AIR));
		for (s16 z = low.z; z < high.z + SMALL.width(); z++)
		for (s16 x = low.x; x < high.x + SMALL.width(); x++) {
			const bool face = SMALL.face(x, z).has_value();
			for (s16 y = 0; y <= (face ? 0 : 20); y++)
				m_map->setNode(v3s16(x, y, z), MapNode(face ? t_CONTENT_STONE : t_CONTENT_BRICK));
		}
	}

	void step(f32 dtime) override {}
	Map &getMap() override { return *m_map; }
	void getSelectedActiveObjects(const core::line3d<f32> &, std::vector<PointedThing> &,
		const std::optional<Pointabilities> &) override {}

private:
	std::unique_ptr<DummyMap> m_map;
};

} // namespace

// A player walking off a face's edge isn't stopped by the gap the net puts
// there: the next face's floor carries on under them, and nothing stands in
// the way. (Carrying them onto that face is the player's: LocalPlayer.)
void TestPlanetMap::testWalkOverEdges(IGameDef *gamedef)
{
	PlanetEnvironment env(gamedef);
	const aabb3f box(-0.3f * BS, 0.0f, -0.3f * BS, 0.3f * BS, 1.7f * BS, 0.3f * BS);
	for (int f = 0; f < 6; f++)
	for (int d = 0; d < 4; d++) {
		int n = 0;
		alongSide(f, d, [&](s16 x, s16 z) {
			if (n++ % 13 != 6)
				return;
			// standing on the edge row's floor, walking off it at 4 m/s for a quarter second
			v3f pos(x * BS, 0.5f * BS, z * BS), speed(DIRS[d].X * 4.0f * BS, 0, DIRS[d].Z * 4.0f * BS);
			const CollisionMoveResult r = collisionMoveSimple(&env, gamedef, box, 0.0f, 0.25f,
				&pos, &speed, v3f(0, -9.81f * BS, 0), nullptr, false, StepUpMode::LEGACY);
			UASSERT(r.touching_ground);
			// a metre on, past the edge, still at the floor's height and still going
			const v3f gone = (pos - v3f(x * BS, 0.5f * BS, z * BS)) / BS;
			UASSERT(std::abs(gone.X - DIRS[d].X * 1.0f) < 0.01f && std::abs(gone.Z - DIRS[d].Z * 1.0f) < 0.01f);
			UASSERT(std::abs(gone.Y) < 0.01f);
			UASSERT(speed.getLength() > 3.9f * BS);
		});
	}
	// Diagonally through each three-face point: collision samples the three
	// incident floors, then the movement rebase lands on a real face.
	for (int f = 0; f < 6; f++) {
		const planet::Planet::Square face = SMALL.square(f);
		for (int dx : { -1, 1 })
		for (int dz : { -1, 1 }) {
			const double x = dx < 0 ? face.x : face.x + SMALL.width() - 1;
			const double z = dz < 0 ? face.z : face.z + SMALL.width() - 1;
			v3f pos(x * BS, 0.5f * BS, z * BS);
			v3f speed(dx * 4.0f * BS, 0, dz * 4.0f * BS);
			const CollisionMoveResult r = collisionMoveSimple(&env, gamedef, box, 0.0f, 0.25f,
					&pos, &speed, v3f(0, -9.81f * BS, 0), nullptr, false, StepUpMode::LEGACY);
			UASSERT(r.touching_ground);
			UASSERT(std::abs(pos.X / BS - (x + dx)) < 0.01f);
			UASSERT(std::abs(pos.Z / BS - (z + dz)) < 0.01f);
			const auto carried = SMALL.carry(f, x, z, pos.X / BS, pos.Z / BS);
			UASSERT(carried && SMALL.face((int32_t)std::lround(carried->x),
					(int32_t)std::lround(carried->z)) == carried->face);
		}
	}
	// the control: the same map read flat walks into the gap's wall
	env.getMap().setPlanet(nullptr);
	const planet::Planet::Square s = SMALL.square(0);
	v3f pos((s.x + SMALL.width() - 1) * BS, 0.5f * BS, (s.z + 40) * BS), speed(4.0f * BS, 0, 0);
	collisionMoveSimple(&env, gamedef, box, 0.0f, 0.25f, &pos, &speed, v3f(0, -9.81f * BS, 0), nullptr, false,
		StepUpMode::LEGACY);
	UASSERT(pos.X / BS < s.x + SMALL.width() - 1 + 0.25f && speed.X == 0);
}

// Looking across a face's edge at a block two nodes past it: the ray goes
// on onto the next face (not into the gap's wall) and points at the block
// where the map keeps it, so digging and placing reach it.
void TestPlanetMap::testPointAcrossEdges(IGameDef *gamedef)
{
	PlanetEnvironment env(gamedef);
	Map &map = env.getMap();
	for (int f = 0; f < 6; f++)
	for (int d = 0; d < 4; d++) {
		int n = 0;
		alongSide(f, d, [&](s16 x, s16 z) {
			if (n++ % 17 != 8)
				return;
			const v3s16 edge(x, 1, z);
			const Map::Next one = map.next(edge, DIRS[d]);
			const auto on = planet::Planet::turned(DIRS[d].X, DIRS[d].Z, one.turn);
			const v3s16 target = one.pos + v3s16(on[0], 0, on[1]); // two past the edge, on the next face
			map.setNode(target, MapNode(t_CONTENT_BRICK));
			// from a node back from the edge, straight across
			const v3f eye = intToFloat(edge - DIRS[d], BS); // level with the block
			const v3f along = intToFloat(DIRS[d], BS) * 6.0f;
			RaycastState state(core::line3d<f32>(eye, eye + along), false, false, std::nullopt);
			PointedThing hit;
			env.continueRaycast(&state, &hit);
			UASSERT(hit.type == POINTEDTHING_NODE);
			UASSERT(hit.node_undersurface == target);
			map.setNode(target, MapNode(CONTENT_AIR));
		});
	}
}

void TestPlanetMap::testCornerNeighbourhood(IGameDef *gamedef)
{
	PlanetEnvironment env(gamedef);
	Map &map = env.getMap();
	for (int face = 0; face < 6; face++) {
		const planet::Planet::Square s = SMALL.square(face);
		for (int dx : { -1, 1 })
		for (int dz : { -1, 1 }) {
			const v3s16 corner(dx < 0 ? s.x : s.x + SMALL.width() - 1, 1,
					dz < 0 ? s.z : s.z + SMALL.width() - 1);
			const v3s16 ghost = corner + v3s16(dx, 0, dz);
			const Map::Next seen = map.see(corner, ghost);
			UASSERT(seen.pos != ghost);
			const auto target_face = SMALL.face(seen.pos.X, seen.pos.Z);
			UASSERT(target_face && *target_face != face);
			map.setNode(seen.pos, MapNode(t_CONTENT_BRICK));
			bool valid = false;
			UASSERTEQ(content_t, map.getNodeSeen(corner, ghost, &valid).getContent(),
					t_CONTENT_BRICK);
			UASSERT(valid);
			map.setNode(seen.pos, MapNode(CONTENT_AIR));
		}
	}
}

// A torch on a face's edge lights the next face's ground beyond it, one
// level less a node, as on flat ground; and taking it away takes the light.
void TestPlanetMap::testLightAcrossEdges(IGameDef *gamedef)
{
	PlanetEnvironment env(gamedef);
	Map &map = env.getMap();
	const NodeDefManager *ndef = gamedef->ndef();
	const int torch = LIGHT_MAX - 1; // test:torch's light_source
	for (int f = 0; f < 6; f++)
	for (int d = 0; d < 4; d++) {
		int n = 0;
		alongSide(f, d, [&](s16 x, s16 z) {
			if (n++ % 29 != 14)
				return;
			const v3s16 edge(x, 3, z);
			const Map::Next one = map.next(edge, DIRS[d]);
			const auto on = planet::Planet::turned(DIRS[d].X, DIRS[d].Z, one.turn);
			const v3s16 two = one.pos + v3s16(on[0], 0, on[1]);
			std::map<v3s16, MapBlock *> modified;
			std::vector<std::pair<v3s16, MapNode>> old{ { edge, map.getNode(edge) } };
			map.setNode(edge, MapNode(t_CONTENT_TORCH));
			voxalgo::update_lighting_nodes(&map, old, modified);
			const auto light = [&](v3s16 p) { return map.getNode(p).getLight(LIGHTBANK_NIGHT, ndef->getLightingFlags(map.getNode(p))); };
			UASSERTEQ(int, light(one.pos), torch - 1);
			UASSERTEQ(int, light(two), torch - 2);
			old = { { edge, map.getNode(edge) } };
			map.setNode(edge, MapNode(CONTENT_AIR));
			voxalgo::update_lighting_nodes(&map, old, modified);
			UASSERTEQ(int, light(one.pos), 0);
		});
	}
}
