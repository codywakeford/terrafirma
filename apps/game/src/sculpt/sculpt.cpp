// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "sculpt.h"

#include "map.h"
#include "nodedef.h"

#include <cmath>
#include <algorithm>
#include <map>
#include <unordered_map>
#include <unordered_set>

namespace sculpt {

using shape::STEPS;

const Kind KINDS[size_t(Tool::Count)] = {
	{ "box", "Box", true, false, 1, Kind::Round::None },
	{ "cylinder", "Cylinder", true, false, 1, Kind::Round::Across },
	{ "cone", "Cone", true, false, 1, Kind::Round::Taper },
	{ "sphere", "Sphere", true, false, 1, Kind::Round::Ball },
	{ "cut", "Cut", true, true, 1, Kind::Round::None },
	{ "bore", "Bore", false, false, 2, Kind::Round::Across },
};

namespace {

// floor(a / b) for b > 0
s32 below(s32 a, s32 b)
{
	return a >= 0 ? a / b : -((-a + b - 1) / b);
}

} // namespace

v3s32 world(const shape::Sixteenth &s)
{
	return v3s32(s.node.X, s.node.Y, s.node.Z) * STEPS + v3s32(s.cell.X, s.cell.Y, s.cell.Z);
}

shape::Sixteenth local(v3s32 w)
{
	const v3s32 n(below(w.X, STEPS), below(w.Y, STEPS), below(w.Z, STEPS));
	const v3s32 c = w - n * STEPS;
	return { v3s16(n.X, n.Y, n.Z), v3s16(c.X, c.Y, c.Z) };
}

bool Stroke::valid() const
{
	const int unit = std::abs(normal.X) + std::abs(normal.Y) + std::abs(normal.Z);
	if (!(tool < Tool::Count && size >= 1 && size <= MAX_SIZE && unit == 1 && shape::Shape::inside(at.cell)
			&& act < Act::Count && (act != Act::Add || KINDS[size_t(tool)].places)))
		return false;
	if (deep && (*deep < 1 || *deep > MAX_DEEP))
		return false;
	if (!to)
		return true;
	if (!shape::Shape::inside(to->cell))
		return false;
	const int a = normal.X ? 0 : normal.Y ? 1 : 2;
	const v3s32 g = world(at), t = world(*to);
	for (int b = 0; b < 3; b++)
		if (b == a ? t[b] != g[b] : std::abs(t[b] - g[b]) >= MAX_SIZE)
			return false;
	return true;
}

std::string Stroke::measure() const
{
	auto words = [](s32 n) {
		return n % STEPS == 0 ? std::to_string(n / STEPS) + (n == STEPS ? " block" : " blocks")
				: std::to_string(n) + "/16";
	};
	const int a = normal.X ? 0 : normal.Y ? 1 : 2;
	const v3s32 g = world(at), t = to ? world(*to) : g;
	std::string out;
	for (int b = 0; b < 3; b++) {
		if (b == a)
			continue;
		out += (out.empty() ? "" : " × ") + words(to ? std::abs(t[b] - g[b]) + 1 : size);
	}
	return out + ", " + words(deep ? *deep : KINDS[size_t(tool)].depth * size) + " deep";
}

std::vector<v3s32> Stroke::cells() const
{
	const Kind &k = KINDS[size_t(tool)];
	const v3s32 g = world(at);
	const int a = normal.X ? 0 : normal.Y ? 1 : 2;
	const int sign = normal.X + normal.Y + normal.Z;
	const s32 depth = deep ? *deep : k.depth * size;
	const bool place = act == Act::Add;
	const bool outward = k.outward ? !place : place;
	const bool snap = size % STEPS == 0;

	const bool dragged = to.has_value();
	const v3s32 t = dragged ? world(*to) : g;
	// a round one's dragged from its middle out (sprint 65): a circle, as
	// far out as it's dragged the furthest way, centred where it started
	s32 radius = 0;
	for (int b = 0; b < 3; b++)
		if (b != a)
			radius = std::max(radius, std::abs(t[b] - g[b]));
	v3s32 lo, hi; // the volume's box, [lo, hi)
	for (int b = 0; b < 3; b++) {
		if (b == a)
			continue;
		if (dragged && k.round != Kind::Round::None) {
			lo[b] = g[b] - radius;
			hi[b] = g[b] + radius + 1;
			continue;
		}
		if (dragged) {
			// the footprint dragged out, corner to corner (to the node grid's
			// lines at a whole-block size)
			lo[b] = std::min(g[b], t[b]);
			hi[b] = std::max(g[b], t[b]) + 1;
			if (snap) {
				lo[b] = below(lo[b], STEPS) * STEPS;
				hi[b] = (below(hi[b] - 1, STEPS) + 1) * STEPS;
			}
			continue;
		}
		// centred on the pointed sixteenth, or on its node
		lo[b] = snap ? (below(g[b], STEPS) - (size / STEPS - 1) / 2) * STEPS : g[b] - (size - 1) / 2;
		hi[b] = lo[b] + size;
	}
	// along the normal: from the pointed sixteenth's face (or its node's)
	const s32 inner = snap ? below(g[a], STEPS) * STEPS + (sign > 0 ? STEPS : 0) : g[a] + (sign > 0 ? 1 : 0);
	if (outward == (sign > 0)) {
		lo[a] = inner;
		hi[a] = inner + depth;
	} else {
		lo[a] = inner - depth;
		hi[a] = inner;
	}

	// the round ones: within the ellipse (ellipsoid) the box holds, from
	// its middle; the taper's shrinks to nothing at the box's far side
	const v3f mid = (v3f(lo.X, lo.Y, lo.Z) + v3f(hi.X, hi.Y, hi.Z)) / 2;
	const v3f half = (v3f(hi.X, hi.Y, hi.Z) - v3f(lo.X, lo.Y, lo.Z)) / 2;
	std::vector<v3s32> out;
	auto add = [&](v3s32 c) {
		out.push_back(c);
		if (mirror) {
			c[mirror->axis] = mirror->twice - 1 - c[mirror->axis];
			out.push_back(c);
		}
	};
	for (s32 x = lo.X; x < hi.X; x++)
		for (s32 y = lo.Y; y < hi.Y; y++)
			for (s32 z = lo.Z; z < hi.Z; z++) {
				if (k.round != Kind::Round::None) {
					const v3f c(x + 0.5f, y + 0.5f, z + 0.5f);
					f32 e = 0; // squared, 1 at the ellipse
					for (int b = 0; b < 3; b++)
						if (b != a || k.round == Kind::Round::Ball)
							e += (c[b] - mid[b]) * (c[b] - mid[b]) / (half[b] * half[b]);
					// the taper: its ellipse scaled by how far it's still to go
					const f32 left = k.round == Kind::Round::Taper ? 1 - std::abs(c[a] - inner) / depth : 1;
					if (e > left * left)
						continue;
				}
				add(v3s32(x, y, z));
			}
	if (mirror) { // where the two meet, once
		std::sort(out.begin(), out.end(), [](const v3s32 &a, const v3s32 &b) {
			return a.X != b.X ? a.X < b.X : a.Y != b.Y ? a.Y < b.Y : a.Z < b.Z;
		});
		out.erase(std::unique(out.begin(), out.end()), out.end());
	}
	return out;
}

std::optional<f32> nearestAlong(v3f from, v3f axis, const core::line3d<f32> &ray)
{
	// the nearest points of two lines: along the axis, t
	const v3f d = (ray.end - ray.start).normalize();
	const v3f w = from - ray.start;
	const f32 b = axis.dotProduct(d);
	const f32 den = 1 - b * b;
	if (std::abs(den) < 1e-4f)
		return std::nullopt;
	return (b * d.dotProduct(w) - axis.dotProduct(w)) / den;
}

std::vector<std::pair<v3s32, v3s32>> outline(const std::vector<v3s32> &cells)
{
	struct Hash
	{
		size_t operator()(const v3s32 &v) const
		{
			return (size_t(u32(v.X)) * 73856093) ^ (size_t(u32(v.Y)) * 19349663) ^ (size_t(u32(v.Z)) * 83492791);
		}
	};
	const std::unordered_set<v3s32, Hash> in(cells.begin(), cells.end());

	// a unit edge along axis e from corner p: the four cells round it; it's
	// an edge of the surface unless they're all in, all out, or two side by
	// side (a flat face)
	// runs[(e, the other two coordinates)] = the edge's starts along e
	std::map<std::array<s32, 3>, std::vector<s32>> runs;
	for (const v3s32 &c : cells)
		for (int e = 0; e < 3; e++) {
			const int u = (e + 1) % 3, v = (e + 2) % 3;
			for (int du = 0; du < 2; du++)
				for (int dv = 0; dv < 2; dv++) {
					v3s32 p = c;
					p[u] += du;
					p[v] += dv;
					bool f[2][2];
					for (int i = 0; i < 2; i++)
						for (int j = 0; j < 2; j++) {
							v3s32 q = p;
							q[u] -= 1 - i;
							q[v] -= 1 - j;
							f[i][j] = in.count(q) > 0;
						}
					const int n = f[0][0] + f[0][1] + f[1][0] + f[1][1];
					const bool flat = n == 2 && f[0][0] != f[1][1];
					if (n == 4 || flat)
						continue;
					runs[{ e, p[u], p[v] }].push_back(p[e]);
				}
		}

	std::vector<std::pair<v3s32, v3s32>> out;
	for (auto &[key, starts] : runs) {
		std::sort(starts.begin(), starts.end());
		starts.erase(std::unique(starts.begin(), starts.end()), starts.end());
		const int e = key[0], u = (e + 1) % 3, v = (e + 2) % 3;
		for (size_t i = 0; i < starts.size();) {
			size_t j = i + 1;
			while (j < starts.size() && starts[j] == starts[j - 1] + 1)
				j++;
			v3s32 a, b;
			a[e] = starts[i];
			b[e] = starts[j - 1] + 1;
			a[u] = b[u] = key[1];
			a[v] = b[v] = key[2];
			out.emplace_back(a, b);
			i = j;
		}
	}
	return out;
}

void Edit::restore(Map &map) const
{
	// each node once, as it was; what didn't change is left be
	for (const Was &w : was) {
		const shape::Shape *now = map.getShape(w.p);
		const bool same_shape = w.shape ? (now && *now == *w.shape) : !now;
		if (map.getNode(w.p) != w.n) {
			if (w.n.getContent() == CONTENT_AIR)
				map.removeNodeWithEvent(w.p);
			else
				map.addNodeWithEvent(w.p, w.n);
		}
		if (!same_shape || (w.shape && !map.getShape(w.p))) // a no-shape (whole) node is a whole shape
			map.setShape(w.p, w.shape ? *w.shape : shape::Shape::whole());
	}
}

void History::record(const std::string &player, Entry entry)
{
	std::deque<Entry> &edits = m_edits[player];
	edits.push_back(std::move(entry));
	if (edits.size() > DEPTH)
		edits.pop_front();
}

void History::shift(u64 machine, u64 part, v3s16 by)
{
	for (auto &[player, edits] : m_edits)
		for (Entry &e : edits)
			if (e.machine == machine && e.part == part)
				for (Edit::Was &w : e.edit.was)
					w.p += by;
}

std::optional<History::Entry> History::pop(const std::string &player)
{
	const auto it = m_edits.find(player);
	if (it == m_edits.end() || it->second.empty())
		return std::nullopt;
	Entry e = std::move(it->second.back());
	it->second.pop_back();
	return e;
}

Edit Sculptor::apply(const Stroke &s, content_t material, std::optional<VoxelArea> within)
{
	const NodeDefManager *ndef = m_map.getNodeDefManager();
	std::unordered_map<v3s16, std::vector<v3s16>> nodes;
	for (const v3s32 &w : s.cells()) {
		const shape::Sixteenth l = local(w);
		if (!within || within->contains(l.node))
			nodes[l.node].push_back(l.cell);
	}

	// what's here before: every node touched, and those settling touches
	// (the one under each, and what it rises into)
	const v3s16 up(0, 1, 0);
	Edit edit;
	std::unordered_set<v3s16> seen;
	for (const auto &[p, cells] : nodes)
		for (const v3s16 q : { p - up, p, p + up }) {
			bool valid;
			const MapNode n = m_map.getNode(q, &valid);
			if (!valid || !seen.insert(q).second)
				continue;
			const shape::Shape *was = m_map.getShape(q);
			edit.was.push_back({ q, n, was ? std::optional<shape::Shape>(*was) : std::nullopt });
		}

	// smoothed ground touched keeps the shape it had: what's under each
	// node first (it may rise into it), then the node
	for (const auto &[p, cells] : nodes) {
		m_map.settleGround(p - up);
		m_map.settleGround(p);
	}

	for (const auto &[p, cells] : nodes) {
		bool valid;
		const MapNode n = m_map.getNode(p, &valid);
		if (!valid)
			continue;
		const ContentFeatures &f = ndef->get(n);
		const shape::Shape *was = m_map.getShape(p);

		if (s.act == Act::Paint) {
			// the node's material changed, its shape kept
			if (f.drawtype != NDT_NORMAL || n.getContent() == material)
				continue;
			const std::optional<shape::Shape> keep = was ? std::optional<shape::Shape>(*was) : std::nullopt;
			m_map.addNodeWithEvent(p, MapNode(material, n.getParam1(), n.getParam2()));
			if (keep)
				m_map.setShape(p, *keep);
			continue;
		}
		if (s.act == Act::Cut) {
			if (f.drawtype != NDT_NORMAL)
				continue;
			shape::Shape cut = was ? *was : shape::Shape::whole();
			for (const v3s16 &c : cells)
				cut.set(c, false);
			if (cut.empty())
				m_map.removeNodeWithEvent(p);
			else if (!was || !(cut == *was))
				m_map.setShape(p, cut);
			continue;
		}

		if (f.buildable_to) {
			shape::Shape fill;
			for (const v3s16 &c : cells)
				fill.set(c, true);
			m_map.addNodeWithEvent(p, MapNode(material));
			m_map.setShape(p, fill);
		} else if (n.getContent() == material && was) {
			shape::Shape fill = *was;
			for (const v3s16 &c : cells)
				fill.set(c, true);
			m_map.setShape(p, fill);
		}
		// else: another material, or whole: one material to a node
	}
	return edit;
}

Edit Sculptor::hollow(const VoxelArea &area, int thickness)
{
	const NodeDefManager *ndef = m_map.getNodeDefManager();
	auto solid = [&](v3s16 p) {
		const MapNode n = m_map.getNode(p);
		return n.getContent() != CONTENT_AIR && ndef->get(n).drawtype == NDT_NORMAL;
	};
	// the solid nodes' box
	std::optional<VoxelArea> box;
	for (s16 x = area.MinEdge.X; x <= area.MaxEdge.X; x++)
		for (s16 y = area.MinEdge.Y; y <= area.MaxEdge.Y; y++)
			for (s16 z = area.MinEdge.Z; z <= area.MaxEdge.Z; z++)
				if (solid({ x, y, z })) {
					if (!box)
						box = VoxelArea(v3s16(x, y, z));
					box->addPoint(v3s16(x, y, z));
				}
	if (!box)
		return {};

	// its sixteenths, and open space a sixteenth round it (all that's
	// further from anything open is at least that far from this)
	const v3s32 origin = v3s32(box->MinEdge.X, box->MinEdge.Y, box->MinEdge.Z) * STEPS - v3s32(1, 1, 1);
	const v3s32 dims = v3s32(box->getExtent().X, box->getExtent().Y, box->getExtent().Z) * STEPS + v3s32(2, 2, 2);
	auto index = [&](v3s32 c) { return size_t(c.X) + size_t(dims.X) * (size_t(c.Y) + size_t(dims.Y) * size_t(c.Z)); };
	std::vector<bool> filled(size_t(dims.X) * dims.Y * dims.Z, false);
	for (s16 x = box->MinEdge.X; x <= box->MaxEdge.X; x++)
		for (s16 y = box->MinEdge.Y; y <= box->MaxEdge.Y; y++)
			for (s16 z = box->MinEdge.Z; z <= box->MaxEdge.Z; z++) {
				const v3s16 p(x, y, z);
				if (!solid(p))
					continue;
				const shape::Shape *cut = m_map.getShape(p);
				for (s16 cx = 0; cx < STEPS; cx++)
					for (s16 cy = 0; cy < STEPS; cy++)
						for (s16 cz = 0; cz < STEPS; cz++)
							if (!cut || cut->has({ cx, cy, cz }))
								filled[index(world({ p, v3s16(cx, cy, cz) }) - origin)] = true;
			}

	// how far each solid sixteenth is from open space (a step to any of
	// the 26 round it), out to the wall's thickness
	std::vector<u8> near(filled.size(), 0); // 0: not reached
	std::vector<v3s32> ring;
	for (s32 z = 0; z < dims.Z; z++)
		for (s32 y = 0; y < dims.Y; y++)
			for (s32 x = 0; x < dims.X; x++)
				if (!filled[index({ x, y, z })])
					ring.emplace_back(x, y, z);
	for (int d = 1; d <= thickness && !ring.empty(); d++) {
		std::vector<v3s32> next;
		for (const v3s32 &c : ring)
			for (s32 dz = -1; dz <= 1; dz++)
				for (s32 dy = -1; dy <= 1; dy++)
					for (s32 dx = -1; dx <= 1; dx++) {
						const v3s32 q = c + v3s32(dx, dy, dz);
						if (q.X < 0 || q.Y < 0 || q.Z < 0 || q.X >= dims.X || q.Y >= dims.Y || q.Z >= dims.Z)
							continue;
						const size_t i = index(q);
						if (filled[i] && !near[i]) {
							near[i] = u8(d);
							next.push_back(q);
						}
					}
		ring = std::move(next);
	}

	// what's further than the wall is taken, node by node
	Edit edit;
	for (s16 x = box->MinEdge.X; x <= box->MaxEdge.X; x++)
		for (s16 y = box->MinEdge.Y; y <= box->MaxEdge.Y; y++)
			for (s16 z = box->MinEdge.Z; z <= box->MaxEdge.Z; z++) {
				const v3s16 p(x, y, z);
				if (!solid(p))
					continue;
				const shape::Shape *was = m_map.getShape(p);
				shape::Shape left = was ? *was : shape::Shape::whole();
				bool changed = false;
				for (s16 cx = 0; cx < STEPS; cx++)
					for (s16 cy = 0; cy < STEPS; cy++)
						for (s16 cz = 0; cz < STEPS; cz++) {
							const size_t i = index(world({ p, v3s16(cx, cy, cz) }) - origin);
							if (filled[i] && !near[i]) {
								left.set({ cx, cy, cz }, false);
								changed = true;
							}
						}
				if (!changed)
					continue;
				edit.was.push_back({ p, m_map.getNode(p), was ? std::optional<shape::Shape>(*was) : std::nullopt });
				if (left.empty())
					m_map.removeNodeWithEvent(p);
				else
					m_map.setShape(p, left);
			}
	return edit;
}

} // namespace sculpt
