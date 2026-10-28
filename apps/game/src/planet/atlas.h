// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "terragen/atlas.h"
#include <array>
#include <cmath>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <vector>

namespace planet {

/*
	The atlas (terragen::Atlas, planet.paper.md) in the engine's map: where
	each of its cells is stored, as a node.

	The rule the layout keeps, as the cube's net does: a plain step from a
	stored node (p + dir) is the true neighbour or the seam, never some
	other cell. Every region's grid (a segment, a cap, the core) is one box
	of the map, walled by a map block of seam: x its i (east), y its k (up:
	the layer, so the sea is where the engine expects it), z its j (north).
	A grid's trimmed cells are seam too. Boxes are packed in rows across
	the map.

	Crossing between regions is the atlas's (terragen::Atlas::neighbours):
	Map::next and Map::see go through it, never p + dir.
*/
class Atlas
{
public:
	using Cell = terragen::Atlas::Cell;

	static constexpr int32_t ORIGIN = -30912; // the lowest node the map generator makes
	static constexpr int32_t SPAN = 61808;    // nodes it makes across, from there
	static constexpr int32_t GAP = 16;        // seam between regions: a map block

	// A node of the map (the engine's v3s16).
	struct Node
	{
		int32_t x, y, z;
	};

	struct Rect
	{
		int32_t x, z, width, depth;
	};

	// One region's grid's storage: its box (x, z; y is the layer, k0 to k1
	// inclusive) and the grid cell at its corner.
	struct Region
	{
		terragen::Atlas::Region kind;
		int index;
		Rect rect;
		int32_t k0, k1;
		int64_t i0, j0;
	};

	explicit Atlas(const terragen::Atlas::Params &params); // throws if it doesn't fit

	const terragen::Atlas &atlas() const { return m_atlas; }
	const std::vector<Region> &regions() const { return m_regions; }

	// The cell a node stores, or none (seam, or off the atlas).
	std::optional<Cell> cell(Node node) const;
	// The node a cell is stored at. Throws for a cell that isn't stored
	// (trimmed, or above the atlas).
	Node node(const Cell &cell) const;

	/*
		Crossing. Each cell's grid carries on past its region (its chart:
		terragen::Atlas::at), so "one step east of here", "two over and one
		up", and "where this mover now is" mean something across any seam.
		Positions are in storage axes (x, y, z; a node's centre is its integer
		position), and `turn` is in quarter turns, east toward north: a
		direction (dx, dz) of the near grid is turned(dx, dz, turn) in the far one (for node
		rotation and light); `angle` is the same turn, exact, in radians (for
		velocities: rotate (x, z) by it, x toward z).
	*/
	struct Step
	{
		Node node;
		int turn;
	};
	// A direction along the ground turned by `turn` quarter turns, east
	// toward north: (dx, dz) of one grid in the next one's.
	template <typename T>
	static std::array<T, 2> turned(T dx, T dz, int turn)
	{
		for (int i = 0; i < ((turn % 4) + 4) % 4; i++) {
			const T t = dx;
			dx = -dz;
			dz = t;
		}
		return { dx, dz };
	}
	// The node across a seam one step along a storage axis (0 x, 1 y, 2 z):
	// the cell straight through the face's middle (of perhaps several across
	// a T-junction). None when the plain step is honest (the same grid, or
	// seam with nothing beyond: the top of the atlas), or `from` stores no cell.
	std::optional<Step> step(Node from, int axis, int sign) const;

	struct Seen
	{
		Node node;
		int turn;
		double x, y, z; // where that node's centre is in `from`'s grid (storage axes)
	};
	// What's at `p` in `from`'s grid carried on: the node there, and where
	// its centre is in that grid (a T-junction's columns don't line up with
	// p). None when p is itself: in `from`'s own grid, or `from` stores no
	// cell. Past the top of the atlas, a seam node.
	std::optional<Seen> see(Node from, Node p) const;
	// The same at any point of `from`'s grid: always an answer when `from`
	// stores a cell (its own nodes included).
	std::optional<Seen> seeAt(Node from, double x, double y, double z) const;

	/*
		Map blocks (16 nodes a side; positions in blocks). What a client is
		sent and what it rebuilds are both found here, so the two agree.
	*/
	static constexpr int32_t BLOCK = 16;
	struct Block
	{
		int32_t x, y, z;
		bool operator==(const Block &o) const { return x == o.x && y == o.y && z == o.z; }
	};
	static Block blockOf(Node n)
	{
		const auto f = [](int32_t v) { return (int32_t)std::floor(v / (double)BLOCK); };
		return { f(n.x), f(n.y), f(n.z) };
	}
	// The blocks a block-sized box of `from`'s grid carried on, centred at
	// (x, y, z), shows: those its middle and its corners fall in (a box over
	// a seam lies across the other grid's blocks), and those across seams
	// from them that reach into it (cut cells between its corners). Seam
	// shows nothing. Kept, by the grid looked from (the same from any of its
	// cells) and the box.
	std::vector<Block> blocksAt(Node from, double x, double y, double z) const;
	// Whether a block and the nodes a step round it are all cells of one
	// grid, clear of its territory's edge (every step from it is plain).
	// Kept.
	bool interior(Block block) const;
	// How a block is drawn: one of its cells (a block holds one grid's cells
	// at most; its chart carries on over the whole block) and its middle on
	// the planet. A mesh's vertices go onto the planet by the chart, less
	// the middle (so they stay small), and the block is drawn moved there.
	// None for a block of no cells. Kept.
	struct BlockChart
	{
		Node cell;
		terragen::Atlas::Vec centre;
	};
	std::optional<BlockChart> blockChart(Block block) const;
	// Storage position (x, y, z) of a block on the planet, from its middle.
	terragen::Atlas::Vec onPlanet(const BlockChart &chart, double x, double y, double z) const
	{
		const terragen::Atlas::Vec p = *point(chart.cell, x, y, z);
		return { p.x - chart.centre.x, p.y - chart.centre.y, p.z - chart.centre.z };
	}

	// What a block's mesh reads past its own grid: each node of the block and
	// its rim that is seam (over a seam, or trimmed) beside a cell of the
	// block, and the cells across it covers (the one straight across first,
	// at `turn`; then the rest it overlaps). Kept.
	struct RimNode
	{
		Node at;
		int turn;
		std::vector<Node> across;
	};
	const std::vector<RimNode> &rim(Block block) const;

	// The blocks across a seam from a block's cells (what blocksAt
	// follows). Kept: the planet never changes.
	const std::vector<Block> &blocksAcross(Block block) const;

	struct Carried
	{
		double x, y, z; // storage position, in the new grid
		double angle;   // radians: rotate a velocity's (x, z) by this, x toward z
	};
	// A mover's node: the one a centimetre above its position (the engine's
	// positions are feet, and feet on a floor are on a node's face; across a
	// seam the node below can be another grid's). Every node taken from a
	// mover's position is this one: collision's, carry's.
	static constexpr double UP = 0.01;
	static Node moverNode(double x, double y, double z)
	{
		return { (int32_t)std::lround(x), (int32_t)std::floor(y + UP + 0.5), (int32_t)std::lround(z) };
	}

	// Something that moved from storage position `was` to (x, y, z), both in
	// the grid it was in: if it has left that grid, where it is in the grid
	// it's in now. None while it's still in its grid, or past the top of the
	// atlas.
	std::optional<Carried> carry(double wasX, double wasY, double wasZ, double x, double y, double z) const;

	// Where storage position (x, y, z) is on the planet, in `from`'s grid
	// carried on. None if `from` stores no cell.
	std::optional<terragen::Atlas::Vec> point(Node from, double x, double y, double z) const;

	/*
		Seeing a region from another: where storage position s of a block's
		grid lands in the eye's grid carried on, as an affine map y = a·s + t
		(a row-major). Exact at the block's centre, first order round it: a
		map block (16 nodes, at most one region's cells) drawn with it is off
		by about 16²/8R nodes at its corners. How the client draws every
		block: the eye's own nearby blocks come out the identity.
	*/
	struct Affine
	{
		std::array<double, 9> a;
		std::array<double, 3> t;
		std::array<double, 3> operator()(double x, double y, double z) const
		{
			return { a[0] * x + a[1] * y + a[2] * z + t[0], a[3] * x + a[4] * y + a[5] * z + t[1],
				a[6] * x + a[7] * y + a[8] * z + t[2] };
		}
	};
	// A grid at a point: where the point is on the planet, and the grid's
	// three storage axes there (its chart's Jacobian). A block's never
	// changes; the eye's does as it moves. None if `from` stores no cell.
	struct Frame
	{
		std::array<double, 3> at; // storage position
		terragen::Atlas::Vec origin;
		std::array<terragen::Atlas::Vec, 3> axes;
	};
	std::optional<Frame> frame(Node from, const std::array<double, 3> &at) const;
	// The view of a block's grid (`block`, at its centre) from the eye's.
	static Affine view(const Frame &eye, const Frame &block);

	/*
		A cut cell's solid: the part of its node inside its territory, cut by
		a plane for each edge of the territory that crosses it (one along a
		seam; two or three at a corner, where a segment's meridians meet a
		cap's circle), in the cell's own storage axes from its node's centre.
		Each edge is flat to about 1e-4 over a cell, and the cells beside it in
		both grids are cut by it: they meet on one surface, no gap and no
		overlap. Drawing, collision and pointing all take this as the cell.
		None for a whole cell, and for a node that stores no cell. Kept.
	*/
	struct Cut
	{
		// d with normal·d + offset >= 0 is kept; |normal| 1
		struct Plane
		{
			std::array<double, 3> normal;
			double offset;

			double at(double x, double y, double z) const { return normal[0] * x + normal[1] * y + normal[2] * z + offset; }
			// The storage axis it most faces.
			int axis() const;
		};
		std::vector<Plane> planes;

		// How far inside a point (storage, from the node's centre) is: >= 0 kept.
		double at(double x, double y, double z) const;
		// Boxes standing for the solid where only boxes will do (collision):
		// the node in `n` × `n` columns along the first plane's axis, each
		// kept to where its middle is. Storage, from the node's centre.
		struct Box
		{
			std::array<double, 3> low, high;
		};
		std::vector<Box> boxes(int n) const;
	};
	std::optional<Cut> cut(Node node) const;
	// A block's cut cells (cut() for the block's every node, found once),
	// each with what its cut faces lie against: for each plane, the nodes
	// just past it at its middle and toward its corners (where those are on
	// the face; none for a face the other planes leave nothing of). Empty
	// for an interior block. Kept: the planet never changes.
	struct CutCell
	{
		Node node;
		Cut cut;
		std::vector<std::vector<Node>> across; // by plane
	};
	using Cuts = std::vector<CutCell>;
	const Cuts &cutsIn(Block block) const;
	// Whether what a block's mesh reads of the atlas (its rim, its cut cells,
	// its chart) is found yet; and finding it (slow at seams: off the main
	// thread).
	bool prepared(Block block) const;
	void prepare(Block block) const;

	// A node that is always seam (above the atlas): what lies past the top.
	Node seam() const;

	// Where a node of a segment or a cap is on the planet: latitude and
	// longitude (radians) of its column (its cell at the surface layer).
	// What the generator builds the column from. None in seam and the core.
	std::optional<std::array<double, 2>> place(Node node) const;

private:
	const Region *regionAt(Node node) const; // its box's region, or none
	int regionOf(const Cell &cell) const;   // m_regions index
	int regionIndex(Node node) const;       // m_regions index, −2 none

	// A cell's chart: its grid continued, d (storage axes) from its centre.
	terragen::Atlas::Vec chart(const Cell &c, double dx, double dy, double dz) const;
	std::array<double, 3> unchart(const Cell &c, const terragen::Atlas::Vec &point) const;
	double turning(const Cell &from, const Cell &to) const; // radians, `to`'s east from `from`'s
	int quarters(const Cell &from, const Cell &to) const;
	int face(const Cell &c, int axis, int sign) const; // storage axis → terragen face

	terragen::Atlas m_atlas;
	std::vector<Region> m_regions;

	// Which nodes of a block and its rim (BLOCK + 2 a side) are cells.
	std::vector<bool> cellsAround(Block block) const;
	std::vector<Block> findAcross(Block block) const;
	std::vector<Block> findAt(Node from, double x, double y, double z) const;
	std::optional<Step> findStep(Node from, const Cell &c, int axis, int sign) const;
	std::optional<BlockChart> findChart(Block block) const;
	std::vector<RimNode> findRim(Block block) const;
	std::optional<Cut> findCut(const Cell &c) const;
	std::vector<std::vector<Node>> findAcross(Node n, const Cut &cut) const;
	// What's found about blocks, kept (the planet never changes; shared by
	// copies of the atlas, and between threads)
	struct Found
	{
		std::mutex lock;
		std::unordered_map<uint64_t, std::vector<Block>> across;     // by block
		std::unordered_map<uint64_t, std::optional<BlockChart>> charts; // by block
		std::unordered_map<uint64_t, std::vector<RimNode>> rims;     // by block
		std::map<std::array<int32_t, 4>, std::optional<Step>> steps; // by node and direction, at seams
		std::map<std::array<int64_t, 4>, std::vector<Block>> at;     // by the grid looked from, and the box's middle
		std::map<std::array<int32_t, 3>, std::optional<Cut>> cuts;   // cells near an edge, by node
		std::unordered_map<uint64_t, std::shared_ptr<const Cuts>> blockCuts; // by block
		std::unordered_map<uint64_t, bool> interior;                 // by block
	};
	std::shared_ptr<Found> m_found = std::make_shared<Found>();
};

} // namespace planet
