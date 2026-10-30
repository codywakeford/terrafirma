// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "piece/machine.h"

#include "exceptions.h"
#include "piece/frame.h"
#include "piece/header.h"
#include "util/serialize.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <sstream>

namespace piece {

const JointKind JOINTS[size_t(Joint::Count)] = {
	{ "fixed", false, 0 },
	{ "rotary", true, 0 },
	{ "hinge", true, 90 },
};

namespace {

const char MAGIC[] = "TFM3";
const char MAGIC_2[] = "TFM2";
const char MAGIC_1[] = "TFM1";
constexpr f32 STEPS = 16; // sixteenths a node

// A part's box (its nodes, local: node n spans n ± 0.5) set in the
// machine's (the same)
std::optional<aabb3f> placed(const std::optional<aabb3f> &local, const Pose &pose)
{
	if (!local)
		return std::nullopt;
	std::optional<aabb3f> out;
	for (int c = 0; c < 8; c++) {
		const v3f corner(c & 1 ? local->MaxEdge.X : local->MinEdge.X, c & 2 ? local->MaxEdge.Y : local->MinEdge.Y,
				c & 4 ? local->MaxEdge.Z : local->MinEdge.Z);
		const v3f at = pose.apply((corner + v3f(0.5f)) * STEPS) / STEPS - v3f(0.5f);
		if (out)
			out->addInternalPoint(at);
		else
			out = aabb3f(at, at);
	}
	return out;
}

std::optional<aabb3f> merged(std::optional<aabb3f> a, const std::optional<aabb3f> &b)
{
	if (!b)
		return a;
	if (!a)
		return b;
	a->addInternalBox(*b);
	return a;
}

struct Texts
{
	std::optional<u64> id;
	struct Part
	{
		Pose pose;
		std::string object;
	};
	std::vector<Part> parts;
	std::vector<Machine::Link> links;
	std::vector<Machine::Socket> sockets;
};

Texts texts(const std::string &text)
{
	const bool first = text.compare(0, 4, MAGIC_1) == 0, second = text.compare(0, 4, MAGIC_2) == 0;
	if (!first && !second && text.compare(0, 4, MAGIC) != 0) // an object's, or a piece's: one part
		return { std::nullopt, { { Pose{ v3s32(0), 0 }, text } }, {}, {} };
	std::istringstream is(text, std::ios::binary);
	is.ignore(4);
	Texts out;
	out.id = readU64(is);
	if (*out.id == 0)
		throw SerializationError("machine: no id");
	out.parts.resize(readU16(is));
	for (Texts::Part &p : out.parts) {
		p.pose.offset = readV3S32(is);
		p.pose.turn = readU8(is);
		turn(p.pose.turn); // throws on one that isn't
		if (first) { // what it was made as, and its radius
			readU8(is);
			readU16(is);
		}
		p.object = deSerializeString32(is);
	}
	out.links.resize(readU16(is));
	for (Machine::Link &l : out.links) {
		l.a = readU64(is);
		l.b = readU64(is);
		const u8 joint = readU8(is);
		if (joint >= u8(Joint::Count))
			throw SerializationError("machine: no such joint");
		l.joint = Joint(joint);
		l.anchor = readV3F32(is);
		l.axis = readV3S16(is);
		if (std::abs(l.axis.X) + std::abs(l.axis.Y) + std::abs(l.axis.Z) != 1)
			throw SerializationError("machine: a joint's axis isn't one");
	}
	if (!first && !second) {
		out.sockets.resize(readU16(is));
		for (Machine::Socket &k : out.sockets) {
			k.id = readU64(is);
			k.part = readU64(is);
			k.point = readV3F32(is);
			k.axis = readV3S16(is);
			const u8 joint = readU8(is);
			if (joint >= u8(Joint::Count))
				throw SerializationError("machine: no such joint for a socket");
			k.joint = Joint(joint);
			if (std::abs(k.axis.X) + std::abs(k.axis.Y) + std::abs(k.axis.Z) != 1)
				throw SerializationError("machine: a socket's axis isn't one");
		}
	}
	return out;
}

} // namespace

v3f Pose::apply(v3f local) const
{
	return piece::turn(turn).apply(local) + v3f(offset.X, offset.Y, offset.Z);
}

v3f Pose::unapply(v3f machine) const
{
	// a square turn's inverse is its transpose
	const Turn &t = piece::turn(turn);
	const v3f d = machine - v3f(offset.X, offset.Y, offset.Z);
	v3f out;
	for (int a = 0; a < 3; a++)
		out[a] = d.dotProduct(v3f::from(t.axes[a]));
	return out;
}

core::matrix4 Pose::matrix() const
{
	// a frame's origin is its node (0, 0, 0)'s middle, half a node in from
	// its corner: the part's taken to the machine's
	const v3f half(STEPS / 2);
	core::matrix4 m = piece::turn(turn).matrix();
	m.setTranslation((apply(half) - half) / STEPS * BS);
	return m;
}

Machine::Machine(IGameDef *gamedef) : m_gamedef(gamedef), m_id(freshId()) {}

void Machine::read(const std::string &text)
{
	Texts t = texts(text);
	std::vector<Part> parts;
	for (const Texts::Part &p : t.parts) {
		Object o(m_gamedef);
		o.read(p.object);
		parts.push_back({ std::move(o), p.pose });
	}
	for (const Link &l : t.links)
		if (std::find_if(parts.begin(), parts.end(), [&](const Part &p) { return p.object.id() == l.a; }) == parts.end()
				|| std::find_if(parts.begin(), parts.end(), [&](const Part &p) { return p.object.id() == l.b; }) == parts.end())
			throw SerializationError("machine: a joint to a part it hasn't");
	for (const Socket &k : t.sockets)
		if (std::find_if(parts.begin(), parts.end(), [&](const Part &p) { return p.object.id() == k.part; }) == parts.end())
			throw SerializationError("machine: a socket on a part it hasn't");
	m_parts = std::move(parts);
	m_links = std::move(t.links);
	m_sockets = std::move(t.sockets);
	// one part from before machines: the machine's id is the object's (as
	// its header says)
	m_id = t.id ? *t.id : m_parts.front().object.id();
}

std::string Machine::write() const
{
	std::ostringstream os(std::ios::binary);
	os.write(MAGIC, 4);
	writeU64(os, m_id);
	writeU16(os, u16(m_parts.size()));
	for (const Part &p : m_parts) {
		writeV3S32(os, p.pose.offset);
		writeU8(os, p.pose.turn);
		os << serializeString32(p.object.write());
	}
	writeU16(os, u16(m_links.size()));
	for (const Link &l : m_links) {
		writeU64(os, l.a);
		writeU64(os, l.b);
		writeU8(os, u8(l.joint));
		writeV3F32(os, l.anchor);
		writeV3S16(os, l.axis);
	}
	writeU16(os, u16(m_sockets.size()));
	for (const Socket &k : m_sockets) {
		writeU64(os, k.id);
		writeU64(os, k.part);
		writeV3F32(os, k.point);
		writeV3S16(os, k.axis);
		writeU8(os, u8(k.joint));
	}
	return os.str();
}

void Machine::renew()
{
	m_id = freshId();
	for (Part &p : m_parts) {
		const u64 was = p.object.id();
		p.object.renew();
		for (Link &l : m_links) {
			if (l.a == was)
				l.a = p.object.id();
			if (l.b == was)
				l.b = p.object.id();
		}
		for (Socket &k : m_sockets)
			if (k.part == was)
				k.part = p.object.id();
	}
	for (Socket &k : m_sockets)
		k.id = freshId();
}

Machine::Part *Machine::part(u64 id)
{
	for (Part &p : m_parts)
		if (p.object.id() == id)
			return &p;
	return nullptr;
}

const Machine::Part *Machine::part(u64 id) const
{
	for (const Part &p : m_parts)
		if (p.object.id() == id)
			return &p;
	return nullptr;
}

Machine::Part &Machine::add(Part part)
{
	m_parts.push_back(std::move(part));
	return m_parts.back();
}

Machine::Part Machine::take(u64 id)
{
	const auto it = std::find_if(m_parts.begin(), m_parts.end(), [&](const Part &p) { return p.object.id() == id; });
	if (it == m_parts.end())
		throw BaseException("Machine::take: no part " + std::to_string(id));
	Part out = std::move(*it);
	m_parts.erase(it);
	m_links.erase(std::remove_if(m_links.begin(), m_links.end(), [&](const Link &l) { return l.a == id || l.b == id; }),
			m_links.end());
	m_sockets.erase(std::remove_if(m_sockets.begin(), m_sockets.end(), [&](const Socket &k) { return k.part == id; }),
			m_sockets.end());
	return out;
}

v3f Machine::point(const Face &face) const
{
	const Part *p = part(face.part);
	if (!p)
		throw BaseException("Machine::point: no part " + std::to_string(face.part));
	const int a = face.normal.X ? 0 : face.normal.Y ? 1 : 2;
	const int sign = face.normal.X + face.normal.Y + face.normal.Z;
	// the part's own sixteenths: the cell's middle, on the face
	const v3s32 cell = v3s32(face.at.node.X, face.at.node.Y, face.at.node.Z) * 16
			+ v3s32(face.at.cell.X, face.at.cell.Y, face.at.cell.Z);
	v3f local;
	for (int b = 0; b < 3; b++)
		local[b] = cell[b] + 0.5f;
	local[a] = cell[a] + (sign > 0 ? 1.f : 0.f);
	return p->pose.apply(local);
}

v3s16 Machine::normal(const Face &face) const
{
	const Part *p = part(face.part);
	if (!p)
		throw BaseException("Machine::normal: no part " + std::to_string(face.part));
	const v3s32 n = turn(p->pose.turn).apply(v3s32(face.normal.X, face.normal.Y, face.normal.Z));
	return v3s16(n.X, n.Y, n.Z);
}

v3s16 Machine::makeRoom(u64 id, v3s32 lo, v3s32 hi)
{
	Part *p = part(id);
	if (!p)
		throw BaseException("Machine::makeRoom: no part " + std::to_string(id));
	// what's there and what's wanted, together
	std::optional<aabb3f> box = boxOf(p->object.design().write());
	v3s32 least = lo, most = hi;
	if (box)
		for (int a = 0; a < 3; a++) {
			least[a] = std::min(least[a], s32(std::lround(box->MinEdge[a] + 0.5f)));
			most[a] = std::max(most[a], s32(std::lround(box->MaxEdge[a] - 0.5f)));
		}
	v3s16 by(0, 0, 0);
	for (int a = 0; a < 3; a++) {
		if (most[a] - least[a] >= SIDE)
			throw BaseException("Machine::makeRoom: wider than a part");
		if (least[a] < 0)
			by[a] = s16(-least[a]);
		else if (most[a] >= SIDE)
			by[a] = s16(SIDE - 1 - most[a]);
	}
	if (by == v3s16(0, 0, 0))
		return by;
	p->object.shift(by);
	p->pose.offset -= turn(p->pose.turn).apply(v3s32(by.X, by.Y, by.Z) * 16);
	return by;
}

Machine::Part &Machine::grow(const Face &face, const std::vector<v3s32> &cells, content_t material, Joint joint)
{
	const Part *from = part(face.part);
	if (!from)
		throw BaseException("Machine::grow: no part " + std::to_string(face.part));
	if (cells.empty())
		throw BaseException("Machine::grow: nothing to grow");
	// its grid's corner the least node the cells are in
	v3s32 least(cells[0].X >> 4, cells[0].Y >> 4, cells[0].Z >> 4);
	for (const v3s32 &c : cells)
		for (int a = 0; a < 3; a++)
			least[a] = std::min(least[a], c[a] >> 4);
	Object o(m_gamedef);
	std::map<v3s16, shape::Shape> nodes;
	for (const v3s32 &c : cells) {
		const v3s32 l = c - least * 16;
		const v3s16 node(l.X >> 4, l.Y >> 4, l.Z >> 4);
		if (node.X >= SIDE || node.Y >= SIDE || node.Z >= SIDE)
			throw BaseException("Machine::grow: wider than a part");
		nodes[node].set(v3s16(l.X & 15, l.Y & 15, l.Z & 15), true);
	}
	for (const auto &[n, s] : nodes) {
		o.design().map().setNode(n, MapNode(material));
		o.design().map().setShape(n, s);
	}
	o.build();
	const v3s32 offset = from->pose.offset + turn(from->pose.turn).apply(least * 16);
	const u64 id = o.id();
	v3f at = point(face);
	const v3s16 n = normal(face);
	if (JOINTS[size_t(joint)].turns) {
		// its axle through the middle of what's drawn, not where the drag
		// began (a wheel turns about its centre)
		v3f lo(cells[0].X, cells[0].Y, cells[0].Z), hi = lo;
		for (const v3s32 &c : cells) {
			lo = v3f(std::min(lo.X, (f32)c.X), std::min(lo.Y, (f32)c.Y), std::min(lo.Z, (f32)c.Z));
			hi = v3f(std::max(hi.X, (f32)c.X), std::max(hi.Y, (f32)c.Y), std::max(hi.Z, (f32)c.Z));
		}
		const v3f middle = from->pose.apply((lo + hi + v3f(1)) * 0.5f);
		for (int a = 0; a < 3; a++)
			if (n[a] == 0)
				at[a] = middle[a];
	}
	m_parts.push_back({ std::move(o), Pose{ offset, from->pose.turn } });
	m_links.push_back({ face.part, id, joint, at, n });
	return m_parts.back();
}

std::vector<std::vector<u64>> Machine::rigid() const
{
	// each part's group, by index: its first part's, joined by welds; what
	// no joint reaches is welded to the first part (it moved with it
	// before there were joints)
	const size_t n = m_parts.size();
	std::vector<size_t> root(n);
	for (size_t i = 0; i < n; i++)
		root[i] = i;
	auto find = [&](size_t i) {
		while (root[i] != i)
			i = root[i] = root[root[i]];
		return i;
	};
	auto weld = [&](size_t a, size_t b) {
		a = find(a), b = find(b);
		root[std::max(a, b)] = std::min(a, b); // the earlier part leads: the same order every time
	};
	auto index = [&](u64 id) {
		for (size_t i = 0; i < n; i++)
			if (m_parts[i].object.id() == id)
				return i;
		throw BaseException("Machine::rigid: a joint to no part " + std::to_string(id));
	};
	std::vector<bool> turning(n, false);
	for (const Link &l : m_links) {
		const size_t a = index(l.a), b = index(l.b);
		if (JOINTS[size_t(l.joint)].turns)
			turning[a] = turning[b] = true;
		else
			weld(a, b);
	}
	for (size_t i = 1; i < n; i++)
		if (!turning[i] && find(i) != 0) {
			bool welded = false;
			for (const Link &l : m_links)
				welded |= index(l.a) == i || index(l.b) == i;
			if (!welded)
				weld(0, i);
		}
	std::vector<std::vector<u64>> groups;
	std::vector<size_t> group(n, n);
	for (size_t i = 0; i < n; i++) {
		const size_t r = find(i);
		if (group[r] == n) {
			group[r] = groups.size();
			groups.emplace_back();
		}
		groups[group[r]].push_back(m_parts[i].object.id());
	}
	return groups;
}

void Machine::relink(u64 a, u64 b, Joint joint, const std::optional<Face> &at)
{
	if (!part(a) || !part(b) || a == b)
		throw BaseException("Machine::relink: not two of its parts");
	for (Link &l : m_links)
		if ((l.a == a && l.b == b) || (l.a == b && l.b == a)) {
			l.joint = joint;
			if (at) {
				l.anchor = point(*at);
				l.axis = normal(*at);
			}
			return;
		}
	if (at)
		m_links.push_back({ a, b, joint, point(*at), normal(*at) });
	else
		m_links.push_back({ a, b, joint, v3f(part(a)->pose.offset.X, part(a)->pose.offset.Y, part(a)->pose.offset.Z),
			v3s16(0, 1, 0) });
}

void Machine::unlink(u64 a, u64 b)
{
	m_links.erase(std::remove_if(m_links.begin(), m_links.end(), [&](const Link &l) {
		return (l.a == a && l.b == b) || (l.a == b && l.b == a);
	}), m_links.end());
}

std::optional<Machine::Face> Machine::faceToward(v3s16 d) const
{
	const std::optional<aabb3f> box = designed();
	if (!box)
		return std::nullopt;
	const v3f dir = v3f::from(d);
	// its middle (sixteenths), from what's filled, not its nodes' box: a
	// wheel 3½ nodes across is met at its middle (sprint 65)
	v3f lo(1e9f), hi(-1e9f);
	for (const Part &p : m_parts)
		p.object.design().each([&](v3s16 n, MapNode, const shape::Shape *s) {
			for (s16 x = 0; x < STEPS; x++)
				for (s16 y = 0; y < STEPS; y++)
					for (s16 z = 0; z < STEPS; z++) {
						if (s && !s->has(v3s16(x, y, z)))
							continue;
						const v3f c = p.pose.apply(v3f(n.X * STEPS + x + 0.5f, n.Y * STEPS + y + 0.5f, n.Z * STEPS + z + 0.5f));
						lo = v3f(std::min(lo.X, c.X), std::min(lo.Y, c.Y), std::min(lo.Z, c.Z));
						hi = v3f(std::max(hi.X, c.X), std::max(hi.Y, c.Y), std::max(hi.Z, c.Z));
					}
		});
	const v3f middle = (lo + hi) / 2;
	std::optional<Face> best;
	f32 best_far = 0, best_off = 0;
	for (const Part &p : m_parts) {
		// the part's own way along d
		const Turn &t = turn(p.pose.turn);
		v3s16 local;
		for (int a = 0; a < 3; a++)
			local[a] = s16(d.X * t.axes[a].X + d.Y * t.axes[a].Y + d.Z * t.axes[a].Z);
		p.object.design().each([&](v3s16 n, MapNode, const shape::Shape *) {
			const v3f centre = p.pose.apply(v3f(n.X, n.Y, n.Z) * STEPS + v3f(STEPS / 2));
			const f32 far = centre.dotProduct(dir);
			const v3f across = (centre - middle) - dir * (centre - middle).dotProduct(dir);
			const f32 off = across.getLengthSQ();
			if (best && (far < best_far - 0.5f || (far < best_far + 0.5f && off >= best_off)))
				return;
			// across, the sixteenth nearest its middle (in the node)
			const v3f there = p.pose.unapply(middle) - v3f(n.X, n.Y, n.Z) * STEPS;
			v3s16 cell;
			for (int a = 0; a < 3; a++)
				cell[a] = local[a] ? (local[a] > 0 ? 15 : 0) : s16(core::clamp<f32>(std::floor(there[a]), 0, STEPS - 1));
			best = Face{ p.object.id(), { n, cell }, local };
			best_far = far;
			best_off = off;
		});
	}
	return best;
}

Machine::Face Machine::mirrored(const Face &face, int axis, s32 twice) const
{
	const Part *p = part(face.part);
	if (!p)
		throw BaseException("Machine::mirrored: no part " + std::to_string(face.part));
	const Pose &pose = p->pose;
	const v3s32 cell = v3s32(face.at.node.X, face.at.node.Y, face.at.node.Z) * 16
			+ v3s32(face.at.cell.X, face.at.cell.Y, face.at.cell.Z);
	v3f middle = pose.apply(v3f(cell.X, cell.Y, cell.Z) + v3f(0.5f));
	middle[axis] = twice - middle[axis];
	const v3f back = pose.unapply(middle);
	const v3s32 w(std::floor(back.X), std::floor(back.Y), std::floor(back.Z));
	const v3s32 node(w.X >> 4, w.Y >> 4, w.Z >> 4); // floor: whole nodes, and what's left
	// the normal mirrored: turned into the machine's, flipped, turned back
	v3s32 n = turn(pose.turn).apply(v3s32(face.normal.X, face.normal.Y, face.normal.Z));
	n[axis] = -n[axis];
	const Turn &t = turn(pose.turn);
	v3s16 local;
	for (int a = 0; a < 3; a++)
		local[a] = s16(n.X * t.axes[a].X + n.Y * t.axes[a].Y + n.Z * t.axes[a].Z);
	return { face.part, { v3s16(node.X, node.Y, node.Z), v3s16(w.X - node.X * 16, w.Y - node.Y * 16, w.Z - node.Z * 16) },
		local };
}

void Machine::join(const Face &mine, Machine other, const Face &theirs, Joint joint, u8 spin)
{
	const v3f p = point(mine);
	const v3s16 n = normal(mine);
	adopt(std::move(other), theirs, p, n, spin);
	m_links.push_back({ mine.part, theirs.part, joint, p, n });
}

void Machine::adopt(Machine other, const Face &theirs, v3f p, v3s16 n, u8 spin)
{
	const v3f q = other.point(theirs);
	const v3s16 m = other.normal(theirs);
	// their face's normal turned against ours, then spun about it; their
	// point on ours
	u8 t = turnTaking(m, v3s16(-n.X, -n.Y, -n.Z), 0);
	const u8 quarter = quarterAbout(n);
	for (u8 i = 0; i < spin % 4; i++)
		t = compose(t, quarter);
	const Turn &turned = turn(t);
	const v3f shift = p - turned.apply(q);
	const v3s32 by(std::lround(shift.X), std::lround(shift.Y), std::lround(shift.Z));
	auto axis = [&](v3s16 a) {
		const v3s32 v = turned.apply(v3s32(a.X, a.Y, a.Z));
		return v3s16(v.X, v.Y, v.Z);
	};
	for (Part &part : other.m_parts) {
		part.pose.offset = turned.apply(part.pose.offset) + by;
		part.pose.turn = compose(part.pose.turn, t);
		m_parts.push_back(std::move(part));
	}
	for (Link &l : other.m_links) {
		l.anchor = turned.apply(l.anchor) + v3f(by.X, by.Y, by.Z);
		l.axis = axis(l.axis);
		m_links.push_back(l);
	}
	for (Socket &k : other.m_sockets) {
		k.point = turned.apply(k.point) + v3f(by.X, by.Y, by.Z);
		k.axis = axis(k.axis);
		m_sockets.push_back(k);
	}
}

const Machine::Socket &Machine::socket(const Face &face, Joint joint)
{
	m_sockets.push_back({ freshId(), face.part, point(face), normal(face), joint });
	return m_sockets.back();
}

void Machine::unsocket(u64 id)
{
	m_sockets.erase(std::remove_if(m_sockets.begin(), m_sockets.end(), [&](const Socket &k) { return k.id == id; }),
			m_sockets.end());
}

void Machine::plug(u64 id, Machine other, const Face &theirs, u8 spin)
{
	const auto it = std::find_if(m_sockets.begin(), m_sockets.end(), [&](const Socket &k) { return k.id == id; });
	if (it == m_sockets.end())
		throw BaseException("Machine::plug: no socket " + std::to_string(id));
	const Socket k = *it;
	m_sockets.erase(it);
	adopt(std::move(other), theirs, k.point, k.axis, spin);
	m_links.push_back({ k.part, theirs.part, k.joint, k.point, k.axis });
}

void Machine::build()
{
	for (Part &p : m_parts)
		p.object.build();
}

bool Machine::empty() const
{
	for (const Part &p : m_parts)
		if (!p.object.empty())
			return false;
	return true;
}

std::optional<aabb3f> Machine::designed() const
{
	std::optional<aabb3f> out;
	for (const Part &p : m_parts)
		out = merged(out, placed(boxOf(p.object.design().write()), p.pose));
	return out;
}

std::optional<aabb3f> Machine::built() const
{
	std::optional<aabb3f> out;
	for (const Part &p : m_parts)
		out = merged(out, placed(boxOf(p.object.built().write()), p.pose));
	return out;
}

Header header(const std::string &text)
{
	const Texts t = texts(text);
	Header out{ t.id, std::nullopt, std::nullopt };
	for (const Texts::Part &p : t.parts) {
		const ObjectText o = split(p.object);
		out.built = merged(out.built, placed(boxOf(o.built), p.pose));
		out.designed = merged(out.designed, placed(boxOf(o.design), p.pose));
		if (!t.id) // one part, from before machines: its id is the object's
			out.id = o.id;
	}
	return out;
}

} // namespace piece
