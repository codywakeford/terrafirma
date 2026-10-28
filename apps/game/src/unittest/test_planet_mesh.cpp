// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "test.h"

#include "client/mesh_generator_thread.h"
#include "dummymap.h"
#include "mapblock.h"
#include "planet/planet.h"

/*
	A block's mesh at a planet face's edge is made with the next face's
	nodes round it (QueuedMeshUpdate::halo), not the seam the net has there.
*/
class TestPlanetMesh : public TestBase {
public:
	TestPlanetMesh() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestPlanetMesh"; }

	void runTests(IGameDef *gamedef);

	void testHaloAtEdges(IGameDef *gamedef);
};

static TestPlanetMesh g_test_instance;

void TestPlanetMesh::runTests(IGameDef *gamedef)
{
	TEST(testHaloAtEdges, gamedef);
}

static const planet::Planet SMALL(64);

void TestPlanetMesh::testHaloAtEdges(IGameDef *gamedef)
{
	const planet::Planet::Square low = SMALL.square(0), high = SMALL.square(5);
	DummyMap map(gamedef, getNodeBlockPos(v3s16(low.x, 0, low.z)),
		getNodeBlockPos(v3s16(high.x + SMALL.width() - 1, 15, high.z + SMALL.width() - 1)));
	map.setPlanet(&SMALL);
	const auto grab = [&](v3s16 block) {
		QueuedMeshUpdate q;
		q.p = block;
		q.retrieveBlocks(&map, 1);
		for (MapBlock *b : q.map_blocks)
			if (b)
				b->refDrop();
		q.map_blocks.clear();
		return q.halo;
	};

	// a block on face 1's east edge, its middle row: brick on face 2 across the edge
	const planet::Planet::Square s = SMALL.square(1);
	const v3s16 edge(s.x + SMALL.width() - 1, 5, s.z + 40);
	const v3s16 across = map.next(edge, v3s16(1, 0, 0)).pos;
	map.setNode(across, MapNode(t_CONTENT_BRICK));
	const auto halo = grab(getNodeBlockPos(edge));
	bool found = false;
	for (const auto &[pos, node] : halo) {
		UASSERT(!SMALL.face(pos.X, pos.Z) || SMALL.face(pos.X, pos.Z) != 1); // only what's off the face
		if (pos == edge + v3s16(1, 0, 0))
			found = node.getContent() == t_CONTENT_BRICK;
	}
	UASSERT(found);
	// At a three-face corner the diagonal halo sample is another incident
	// face, never the net's synthetic seam.
	const v3s16 corner(s.x + SMALL.width() - 1, 5, s.z + SMALL.width() - 1);
	const v3s16 ghost = corner + v3s16(1, 0, 1);
	const v3s16 corner_across = map.see(corner, ghost).pos;
	UASSERT(corner_across != ghost);
	map.setNode(corner_across, MapNode(t_CONTENT_BRICK));
	const auto corner_halo = grab(getNodeBlockPos(corner));
	bool found_corner = false;
	for (const auto &[pos, node] : corner_halo)
		if (pos == ghost)
			found_corner = node.getContent() == t_CONTENT_BRICK;
	UASSERT(found_corner);
	// a block in the middle of a face: nothing to add
	UASSERT(grab(getNodeBlockPos(v3s16(s.x + SMALL.half(), 5, s.z + SMALL.half()))).empty());
}
