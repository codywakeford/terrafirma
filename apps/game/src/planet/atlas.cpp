// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "atlas.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

#include <cstdlib>

namespace planet {

using Kind = terragen::Atlas::Region;

namespace {

constexpr double TAU = 6.283185307179586476925;
constexpr int32_t Y_LOW = -30912, Y_HIGH = 30927; // the generator's limits, as ORIGIN

bool inside(const Atlas::Rect &r, int32_t x, int32_t z)
{
	return x >= r.x && x < r.x + r.width && z >= r.z && z < r.z + r.depth;
}

} // namespace

// Each region's grid a box, in rows across the map, a gap apart; y the layer.
Atlas::Atlas(const terragen::Atlas::Params &params) : m_atlas(params)
{
	const auto refuse = [&](const std::string &what) {
		throw std::invalid_argument("planet::Atlas: " + what + " doesn't fit the map (" + m_atlas.code() + ")");
	};
	int32_t x = ORIGIN, z = ORIGIN, depth = 0;
	for (const terragen::Atlas::Grid &g : m_atlas.grids()) {
		Region r{ g.region, g.index, {}, (int32_t)g.low[2], (int32_t)g.high[2], g.low[0], g.low[1] };
		r.rect.width = (int32_t)(g.high[0] - g.low[0] + 1);
		r.rect.depth = (int32_t)(g.high[1] - g.low[1] + 1);
		if (r.rect.width > SPAN || r.rect.depth > SPAN || r.k0 < Y_LOW || r.k1 >= Y_HIGH)
			refuse("a region");
		if (x + r.rect.width > ORIGIN + SPAN) {
			x = ORIGIN;
			z += depth + GAP;
			depth = 0;
		}
		r.rect.x = x;
		r.rect.z = z;
		if (z + r.rect.depth > ORIGIN + SPAN)
			refuse("the regions, packed");
		x += r.rect.width + GAP;
		depth = std::max(depth, r.rect.depth);
		m_regions.push_back(r);
	}
}

const Atlas::Region *Atlas::regionAt(Node node) const
{
	for (const Region &r : m_regions)
		if (inside(r.rect, node.x, node.z) && node.y >= r.k0 && node.y <= r.k1)
			return &r;
	return nullptr;
}

std::optional<Atlas::Cell> Atlas::cell(Node node) const
{
	const Region *r = regionAt(node);
	if (!r)
		return std::nullopt;
	const Cell c{ r->kind, r->index, r->i0 + (node.x - r->rect.x), r->j0 + (node.z - r->rect.z), node.y };
	return m_atlas.exists(c) ? std::optional(c) : std::nullopt;
}

int Atlas::regionOf(const Cell &c) const
{
	for (size_t n = 0; n < m_regions.size(); n++)
		if (m_regions[n].kind == c.region && m_regions[n].index == c.index)
			return (int)n;
	return -1;
}

Atlas::Node Atlas::node(const Cell &c) const
{
	const int index = regionOf(c);
	if (index < 0)
		throw std::invalid_argument("planet::Atlas: cell has no region");
	const Region &r = m_regions[index];
	const Node n{ (int32_t)(r.rect.x + c.i - r.i0), (int32_t)c.k, (int32_t)(r.rect.z + c.j - r.j0) };
	const std::optional<Cell> back = cell(n);
	if (!back || !(*back == c))
		throw std::invalid_argument("planet::Atlas: cell isn't stored");
	return n;
}

std::optional<std::array<double, 2>> Atlas::place(Node node) const
{
	const Region *r = regionAt(node);
	if (!r || r->kind == Kind::Core)
		return std::nullopt;
	const Cell surface{ r->kind, r->index, r->i0 + (node.x - r->rect.x), r->j0 + (node.z - r->rect.z), 0 };
	const terragen::Atlas::Vec p = m_atlas.centre(surface);
	const double l = std::sqrt(p.x * p.x + p.y * p.y + p.z * p.z);
	return std::array<double, 2>{ std::asin(p.y / l), std::atan2(p.z, p.x) };
}

int Atlas::regionIndex(Node node) const
{
	const Region *r = regionAt(node);
	return r ? (int)(r - m_regions.data()) : -2;
}

std::optional<terragen::Atlas::Vec> Atlas::point(Node from, double x, double y, double z) const
{
	const std::optional<Cell> c = cell(from);
	if (!c)
		return std::nullopt;
	return chart(*c, x - from.x, y - from.y, z - from.z);
}

Atlas::Node Atlas::seam() const
{
	return { ORIGIN, m_atlas.params().height, ORIGIN };
}

/*
	Charts. Storage axes: x the grid's i (east), y its k (up), z its j (north).
*/
terragen::Atlas::Vec Atlas::chart(const Cell &c, double dx, double dy, double dz) const
{
	return m_atlas.at(c, dx, dz, dy);
}

std::array<double, 3> Atlas::unchart(const Cell &c, const terragen::Atlas::Vec &p) const
{
	const std::array<double, 3> g = m_atlas.grid(c.region, c.index, p);
	return { g[0] - c.i - 0.5, g[2] - c.k - 0.5, g[1] - c.j - 0.5 };
}

// How `to`'s grid sits in `from`'s carried on: the angle from `from`'s east
// (its chart's x, there) to `to`'s, about the up at `to`.
double Atlas::turning(const Cell &from, const Cell &to) const
{
	using V = terragen::Atlas::Vec;
	const auto sub = [](V a, V b) { return V{ a.x - b.x, a.y - b.y, a.z - b.z }; };
	const auto dot = [](V a, V b) { return a.x * b.x + a.y * b.y + a.z * b.z; };
	const auto cross = [](V a, V b) { return V{ a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x }; };
	const V middle = m_atlas.centre(to);
	const double l = std::sqrt(dot(middle, middle));
	const V up{ middle.x / l, middle.y / l, middle.z / l };
	const std::array<double, 3> d = unchart(from, middle);
	const V near = sub(chart(from, d[0] + 0.5, d[1], d[2]), chart(from, d[0] - 0.5, d[1], d[2]));
	const V far = sub(chart(to, 0.5, 0, 0), chart(to, -0.5, 0, 0));
	return std::atan2(dot(cross(near, far), up), dot(near, far));
}

// As planet::Planet's turn: a direction (dx, dz) of `from`'s grid is
// Planet::turned(dx, dz, turn) in `to`'s.
int Atlas::quarters(const Cell &from, const Cell &to) const
{
	return (int)std::lround(turning(from, to) / (TAU / 4));
}

int Atlas::face(const Cell &, int axis, int sign) const
{
	// terragen faces: 0/1 i, 2/3 j, 4/5 k; storage x is i, y is k, z is j
	const int along = axis == 0 ? 0 : axis == 1 ? 2 : 1;
	return 2 * along + (sign > 0 ? 1 : 0);
}

static uint64_t blockKey(const Atlas::Block &b)
{
	return ((uint64_t)(uint16_t)b.x << 32) | ((uint64_t)(uint16_t)b.y << 16) | (uint16_t)b.z;
}

std::optional<Atlas::Step> Atlas::step(Node from, int axis, int sign) const
{
	if (interior(blockOf(from)))
		return std::nullopt;
	const std::optional<Cell> c = cell(from);
	if (!c)
		return std::nullopt;
	Node q = from;
	(axis == 0 ? q.x : axis == 1 ? q.y : q.z) += sign;
	const std::optional<Cell> plain = cell(q);
	// the same grid: the plain step is the neighbour; but from or to a cut
	// cell (at a seam), what's straight through the face may be across the
	// edge, and that's the neighbour that counts (from both sides alike, or
	// a liquid flows one way only)
	const bool cut = m_atlas.retained(*c) < 1 || (plain && m_atlas.retained(*plain) < 1);
	if (plain && !cut)
		return std::nullopt;
	// at a seam: found once, kept (the planet never changes)
	const std::array<int32_t, 4> key{ from.x, from.y, from.z, axis * 2 + (sign > 0) };
	{
		std::lock_guard<std::mutex> hold(m_found->lock);
		const auto it = m_found->steps.find(key);
		if (it != m_found->steps.end())
			return it->second;
	}
	const std::optional<Step> found = findStep(from, *c, axis, sign);
	std::lock_guard<std::mutex> hold(m_found->lock);
	return m_found->steps.emplace(key, found).first->second;
}

std::optional<Atlas::Step> Atlas::findStep(Node from, const Cell &c, int axis, int sign) const
{
	Node q = from;
	(axis == 0 ? q.x : axis == 1 ? q.y : q.z) += sign;
	const std::optional<Cell> plain = cell(q);
	// straight through the face's middle (of perhaps several across a
	// T-junction; past a cut's edge where that comes first)
	const std::optional<Cell> through = m_atlas.through(c, face(c, axis, sign));
	if (!through)
		return std::nullopt; // the top of the atlas: seam
	const Cell target = *through;
	if (plain && target == *plain)
		return std::nullopt; // straight through is the plain step after all
	return Step{ node(target), quarters(c, target) };
}

std::optional<Atlas::Seen> Atlas::see(Node from, Node p) const
{
	if (!cell(from) || (regionIndex(p) == regionIndex(from) && cell(p)))
		return std::nullopt;
	return seeAt(from, p.x, p.y, p.z);
}

std::vector<Atlas::Block> Atlas::findAt(Node from, double x, double y, double z) const
{
	std::vector<Block> out;
	const double reach = BLOCK / 2 - 0.01;
	for (int k = 0; k < 9; k++) {
		const double dx = k == 8 ? 0 : (k & 1 ? reach : -reach);
		const double dy = k == 8 ? 0 : (k & 2 ? reach : -reach);
		const double dz = k == 8 ? 0 : (k & 4 ? reach : -reach);
		const std::optional<Seen> s = seeAt(from, x + dx, y + dy, z + dz);
		if (!s || !cell(s->node))
			continue; // seam, or past the top of the atlas
		const Block b = blockOf(s->node);
		if (std::find(out.begin(), out.end(), b) == out.end())
			out.push_back(b);
	}
	// and on across seams from them (another grid's block clipping a corner
	// of the box, between the samples), while a block is near enough to
	// reach into the box: its middle within the two blocks' half diagonals
	// of the box's
	const std::optional<terragen::Atlas::Vec> middle = point(from, x, y, z);
	const double near = BLOCK * std::sqrt(3.0);
	for (size_t i = 0; i < out.size(); i++)
		for (const Block &b : blocksAcross(out[i])) {
			if (std::find(out.begin(), out.end(), b) != out.end())
				continue;
			const std::optional<BlockChart> c = blockChart(b);
			if (!c || !middle)
				continue;
			const double dx = c->centre.x - middle->x, dy = c->centre.y - middle->y, dz = c->centre.z - middle->z;
			if (dx * dx + dy * dy + dz * dz <= near * near)
				out.push_back(b);
		}
	return out;
}

std::optional<Atlas::BlockChart> Atlas::findChart(Block block) const
{
	const Node low{ block.x * BLOCK, block.y * BLOCK, block.z * BLOCK };
	const double middle = BLOCK / 2 - 0.5;
	std::optional<Node> cellIn;
	const Node mid{ low.x + BLOCK / 2, low.y + BLOCK / 2, low.z + BLOCK / 2 };
	if (cell(mid))
		cellIn = mid;
	for (int32_t z = 0; !cellIn && z < BLOCK; z++)
		for (int32_t y = 0; !cellIn && y < BLOCK; y++)
			for (int32_t x = 0; !cellIn && x < BLOCK; x++) {
				const Node n{ low.x + x, low.y + y, low.z + z };
				if (cell(n))
					cellIn = n;
			}
	if (!cellIn)
		return std::nullopt;
	return BlockChart{ *cellIn, *point(*cellIn, low.x + middle, low.y + middle, low.z + middle) };
}

/*
	Kept answers: looked up under the lock, found outside it (another
	thread may find the same meanwhile: the first kept wins; they agree).
*/

std::vector<Atlas::Block> Atlas::blocksAt(Node from, double x, double y, double z) const
{
	// a grid carried on is the same from any of its cells: keyed by the region
	const std::array<int64_t, 4> key{ regionIndex(from), std::llround(x * 2), std::llround(y * 2), std::llround(z * 2) };
	{
		std::lock_guard<std::mutex> hold(m_found->lock);
		const auto it = m_found->at.find(key);
		if (it != m_found->at.end())
			return it->second;
	}
	std::vector<Block> found = findAt(from, x, y, z);
	std::lock_guard<std::mutex> hold(m_found->lock);
	return m_found->at.emplace(key, std::move(found)).first->second;
}

const std::vector<Atlas::Block> &Atlas::blocksAcross(Block block) const
{
	{
		std::lock_guard<std::mutex> hold(m_found->lock);
		const auto it = m_found->across.find(blockKey(block));
		if (it != m_found->across.end())
			return it->second;
	}
	std::vector<Block> found = findAcross(block);
	std::lock_guard<std::mutex> hold(m_found->lock);
	return m_found->across.emplace(blockKey(block), std::move(found)).first->second;
}

const std::vector<Atlas::RimNode> &Atlas::rim(Block block) const
{
	{
		std::lock_guard<std::mutex> hold(m_found->lock);
		const auto it = m_found->rims.find(blockKey(block));
		if (it != m_found->rims.end())
			return it->second;
	}
	std::vector<RimNode> found = findRim(block);
	std::lock_guard<std::mutex> hold(m_found->lock);
	return m_found->rims.emplace(blockKey(block), std::move(found)).first->second;
}

std::vector<Atlas::RimNode> Atlas::findRim(Block block) const
{
	constexpr int32_t W = BLOCK + 2;
	const std::vector<bool> isCell = cellsAround(block);
	const Node low{ block.x * BLOCK - 1, block.y * BLOCK - 1, block.z * BLOCK - 1 };
	const auto at = [&](int32_t x, int32_t y, int32_t z) { return isCell[((size_t)z * W + y) * W + x]; };
	const auto inBlock = [](int32_t v) { return v >= 1 && v <= BLOCK; };

	std::vector<RimNode> out;
	static const int32_t DIRS[6][3] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };
	for (int32_t z = 0; z < W; z++)
		for (int32_t y = 0; y < W; y++)
			for (int32_t x = 0; x < W; x++) {
				if (at(x, y, z))
					continue; // a cell: stored where it is
				for (const auto &d : DIRS) {
					const int32_t fx = x + d[0], fy = y + d[1], fz = z + d[2];
					if (!inBlock(fx) || !inBlock(fy) || !inBlock(fz) || !at(fx, fy, fz))
						continue;
					// seen from the cell of the block it borders: straight across, then
					// every cell its own (from's grid's) cell overlaps there, at its
					// middle and near its corners (a cell is at least ~3/4 of a node wide)
					const Node from{ low.x + fx, low.y + fy, low.z + fz }, q{ low.x + x, low.y + y, low.z + z };
					const Cell c = *cell(from);
					const double ox = q.x - from.x, oy = q.y - from.y, oz = q.z - from.z;
					const terragen::Atlas::Vec there = chart(c, ox, oy, oz);
					const double r = std::sqrt(there.x * there.x + there.y * there.y + there.z * there.z);
					if (r >= m_atlas.params().radius + m_atlas.params().height)
						break; // past the top: nothing across
					const Cell straight = m_atlas.locate(there);
					std::vector<Cell> cells{ straight };
					for (int k = 0; k < 8; k++) {
						const double e = 0.45;
						const terragen::Atlas::Vec p = chart(c, ox + (k & 1 ? e : -e), oy + (k & 2 ? e : -e), oz + (k & 4 ? e : -e));
						if (std::sqrt(p.x * p.x + p.y * p.y + p.z * p.z) >= m_atlas.params().radius + m_atlas.params().height)
							continue;
						const Cell s = m_atlas.locate(p);
						if (std::find(cells.begin(), cells.end(), s) == cells.end())
							cells.push_back(s);
					}
					RimNode rim{ q, quarters(c, straight), {} };
					for (const Cell &s : cells)
						rim.across.push_back(node(s));
					out.push_back(std::move(rim));
					break;
				}
			}
	return out;
}

bool Atlas::interior(Block block) const
{
	{
		std::lock_guard<std::mutex> hold(m_found->lock);
		const auto it = m_found->interior.find(blockKey(block));
		if (it != m_found->interior.end())
			return it->second;
	}
	// its middle's cell more than the block's half diagonal and a step in
	const Node middle{ block.x * BLOCK + BLOCK / 2, block.y * BLOCK + BLOCK / 2, block.z * BLOCK + BLOCK / 2 };
	const std::optional<Cell> c = cell(middle);
	const bool found = c && m_atlas.clearance(*c) > BLOCK + 2 &&
			c->k + BLOCK < m_atlas.params().height;
	std::lock_guard<std::mutex> hold(m_found->lock);
	return m_found->interior.emplace(blockKey(block), found).first->second;
}

std::optional<Atlas::Extent> Atlas::extent(Node n) const
{
	if (interior(blockOf(n)))
		return std::nullopt;
	const std::optional<Cell> c = cell(n);
	// a cell clear of its territory's edge by more than its half diagonal
	if (!c || m_atlas.clearance(*c) > 0.9)
		return std::nullopt;
	const std::array<int32_t, 3> key{ n.x, n.y, n.z };
	{
		std::lock_guard<std::mutex> hold(m_found->lock);
		const auto it = m_found->extents.find(key);
		if (it != m_found->extents.end())
			return it->second;
	}
	const Extent found = findExtent(*c);
	std::lock_guard<std::mutex> hold(m_found->lock);
	return m_found->extents.emplace(key, found).first->second;
}

Atlas::Extent Atlas::findExtent(const Cell &c) const
{
	constexpr int STEPS = 16;
	Extent out{ { -0.5, -0.5, -0.5 }, { 0.5, 0.5, 0.5 } };
	for (int axis = 0; axis < 3; axis++) {
		double low = 1, high = -1;
		for (int s = 0; s <= STEPS; s++) {
			const double t = -0.5 + (double)s / STEPS;
			std::array<double, 3> d{ 0, 0, 0 };
			d[axis] = t;
			if (m_atlas.territory(c, chart(c, d[0], d[1], d[2])) >= 0) {
				low = std::min(low, t);
				high = std::max(high, t);
			}
		}
		if (low > high)
			continue; // the line through the centre is outside: the cell is kept off it, whole along it
		// to halfway to the first sample outside
		out.low[axis] = std::max(-0.5, low - 0.5 / STEPS);
		out.high[axis] = std::min(0.5, high + 0.5 / STEPS);
	}
	// clipped along the one axis the edge most faces (cut shortest): a box
	// clipped along two where the edge runs slantwise loses its corners,
	// and leaves gaps between it and the next grid's cells
	int across = 0;
	for (int axis = 1; axis < 3; axis++)
		if (out.high[axis] - out.low[axis] < out.high[across] - out.low[across])
			across = axis;
	for (int axis = 0; axis < 3; axis++)
		if (axis != across) {
			out.low[axis] = -0.5;
			out.high[axis] = 0.5;
		}
	return out;
}

std::optional<Atlas::BlockChart> Atlas::blockChart(Block block) const
{
	{
		std::lock_guard<std::mutex> hold(m_found->lock);
		const auto it = m_found->charts.find(blockKey(block));
		if (it != m_found->charts.end())
			return it->second;
	}
	std::optional<BlockChart> found = findChart(block);
	std::lock_guard<std::mutex> hold(m_found->lock);
	return m_found->charts.emplace(blockKey(block), found).first->second;
}

std::vector<bool> Atlas::cellsAround(Block block) const
{
	constexpr int32_t W = BLOCK + 2;
	const Node low{ block.x * BLOCK - 1, block.y * BLOCK - 1, block.z * BLOCK - 1 };
	// Its corners' cells all one region's, well inside its territory (two
	// blocks clear: a box this small bulges no further into the core's
	// sphere, and a region's territory is otherwise convex): every node is.
	const Region *region = regionAt(low);
	bool whole = region != nullptr;
	for (int k = 0; whole && k < 8; k++) {
		const Node corner{ low.x + (k & 1 ? W - 1 : 0), low.y + (k & 2 ? W - 1 : 0), low.z + (k & 4 ? W - 1 : 0) };
		const std::optional<Cell> c = regionAt(corner) == region ? cell(corner) : std::nullopt;
		whole = c && m_atlas.clearance(*c) >= 2;
	}
	if (whole)
		return std::vector<bool>((size_t)W * W * W, true);
	std::vector<bool> isCell((size_t)W * W * W);
	for (int32_t z = 0; z < W; z++)
		for (int32_t y = 0; y < W; y++)
			for (int32_t x = 0; x < W; x++)
				isCell[((size_t)z * W + y) * W + x] = cell({ low.x + x, low.y + y, low.z + z }).has_value();
	return isCell;
}

std::vector<Atlas::Block> Atlas::findAcross(Block block) const
{
	// every cell of the block beside a node that isn't one (seam, or trimmed,
	// inside the block or past its faces): the cell across.
	// Which nodes of the block and its rim are cells, found once.
	constexpr int32_t W = BLOCK + 2;
	const std::vector<bool> isCell = cellsAround(block);
	const Node low{ block.x * BLOCK - 1, block.y * BLOCK - 1, block.z * BLOCK - 1 };
	const auto at = [&](int32_t x, int32_t y, int32_t z) { return isCell[((size_t)z * W + y) * W + x]; };

	std::vector<Block> out;
	static const int32_t DIRS[6][3] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };
	for (int32_t z = 1; z <= BLOCK; z++)
		for (int32_t y = 1; y <= BLOCK; y++)
			for (int32_t x = 1; x <= BLOCK; x++) {
				// every other cell (a checkerboard): a block across is wider
				// than one, and its neighbours find it too
				if (!at(x, y, z) || (x + y + z) % 2)
					continue;
				for (int d = 0; d < 6; d++) {
					if (at(x + DIRS[d][0], y + DIRS[d][1], z + DIRS[d][2]))
						continue; // a plain step: the same grid
					// what's just past the face's middle, in that cell's grid
					// carried on (where a block is, not which neighbour exactly)
					const Node n{ low.x + x, low.y + y, low.z + z };
					const std::optional<Seen> s = seeAt(n, n.x + 0.501 * DIRS[d][0], n.y + 0.501 * DIRS[d][1],
							n.z + 0.501 * DIRS[d][2]);
					if (!s || !cell(s->node))
						continue; // past the top
					const Block there = blockOf(s->node);
					if (!(there == block) && std::find(out.begin(), out.end(), there) == out.end())
						out.push_back(there);
				}
			}
	return out;
}

std::optional<Atlas::Seen> Atlas::seeAt(Node from, double x, double y, double z) const
{
	const std::optional<Cell> c = cell(from);
	if (!c)
		return std::nullopt;
	const terragen::Atlas::Vec there = chart(*c, x - from.x, y - from.y, z - from.z);
	const auto &params = m_atlas.params();
	const double r = std::sqrt(there.x * there.x + there.y * there.y + there.z * there.z);
	const Seen nothing{ seam(), 0, x, y, z };
	if (r >= params.radius + params.height)
		return nothing;
	const Cell target = m_atlas.locate(there);
	// the far cell's own centre (its chart's origin), not its corners'
	// average: on a curved cell that sits a hair inside the arc, and a floor
	// seen there would be a ledge
	const std::array<double, 3> at = unchart(*c, chart(target, 0, 0, 0));
	return Seen{ node(target), quarters(*c, target),
		from.x + at[0], from.y + at[1], from.z + at[2] };
}

std::optional<Atlas::Carried> Atlas::carry(double wasX, double wasY, double wasZ, double x, double y, double z) const
{
	const Node from = moverNode(wasX, wasY, wasZ);
	const std::optional<Cell> c = cell(from);
	if (!c)
		return std::nullopt;
	const Node now = moverNode(x, y, z);
	if (regionIndex(now) == regionIndex(from) && cell(now))
		return std::nullopt; // still in its grid
	// located a centimetre up, as moverNode: a point on a face is the node above's
	const terragen::Atlas::Vec there = chart(*c, x - from.x, y + UP - from.y, z - from.z);
	const auto &params = m_atlas.params();
	if (std::sqrt(there.x * there.x + there.y * there.y + there.z * there.z) >= params.radius + params.height)
		return std::nullopt;
	const Cell target = m_atlas.locate(there);
	const Node n = node(target);
	// in the cell's box (a sliver merged into it lies a little outside it:
	// it's the cell's)
	std::array<double, 3> at = unchart(target, there);
	for (double &v : at)
		v = std::clamp(v, -0.499, 0.499);
	return Carried{ n.x + at[0], n.y + at[1] - UP, n.z + at[2], turning(*c, target) };
}

std::optional<Atlas::Frame> Atlas::frame(Node from, const std::array<double, 3> &at) const
{
	const std::optional<terragen::Atlas::Vec> o = point(from, at[0], at[1], at[2]);
	if (!o)
		return std::nullopt;
	Frame f{ at, *o, {} };
	for (int i = 0; i < 3; i++) {
		std::array<double, 3> hi = at, lo = at;
		hi[i] += 0.5;
		lo[i] -= 0.5;
		const terragen::Atlas::Vec h = *point(from, hi[0], hi[1], hi[2]), l = *point(from, lo[0], lo[1], lo[2]);
		f.axes[i] = { h.x - l.x, h.y - l.y, h.z - l.z };
	}
	return f;
}

Atlas::Affine Atlas::view(const Frame &eye, const Frame &block)
{
	using V = terragen::Atlas::Vec;
	const auto cross = [](V a, V b) { return V{ a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x }; };
	const auto dot = [](V a, V b) { return a.x * b.x + a.y * b.y + a.z * b.z; };
	// the eye's axes inverted (rows of its inverse, by cofactors)
	const std::array<V, 3> &je = eye.axes;
	const V r0 = cross(je[1], je[2]), r1 = cross(je[2], je[0]), r2 = cross(je[0], je[1]);
	const double det = dot(je[0], r0);
	const std::array<V, 3> inv{ V{ r0.x / det, r0.y / det, r0.z / det }, V{ r1.x / det, r1.y / det, r1.z / det },
		V{ r2.x / det, r2.y / det, r2.z / det } };

	Affine m;
	for (int r = 0; r < 3; r++)
		for (int c = 0; c < 3; c++)
			m.a[3 * r + c] = dot(inv[r], block.axes[c]);
	const V d{ block.origin.x - eye.origin.x, block.origin.y - eye.origin.y, block.origin.z - eye.origin.z };
	const std::array<double, 3> &c = block.at;
	for (int r = 0; r < 3; r++) {
		// the block's centre lands at the eye plus the eye's grid's way there;
		// the map is a·(s − centre) round it
		const double landed = eye.at[r] + dot(inv[r], d);
		m.t[r] = landed - (m.a[3 * r] * c[0] + m.a[3 * r + 1] * c[1] + m.a[3 * r + 2] * c[2]);
	}
	return m;
}

} // namespace planet
