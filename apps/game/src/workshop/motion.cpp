// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "motion.h"

#include "gamedef.h"
#include "log.h"
#include "map.h"
#include "mapblock.h"
#include "nodedef.h"
#include "object_properties.h"
#include "piece/frame.h"
#include "piece/machine.h"
#include "remoteplayer.h"
#include "server/luaentity_sao.h"
#include "server/player_sao.h"
#include "serverenvironment.h"
#include "settings.h"
#include "util/numeric.h"
#include "util/serialize.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace workshop {

namespace {

// Rolling resistance: what a wheel's axle costs it of its share of the
// weight at its radius (a cart wheel on a road, roughly).
constexpr float ROLLING = 0.02f;
// A hinged group is round if, every way out from its axle, it reaches at
// least this share of as far as it reaches any way.
constexpr float ROUND = 0.85f;
constexpr int ROUND_BINS = 32;
// A player as the physics pushes with them (nodes): a little wider than
// they collide, so what they walk into is pushed before they're stopped.
constexpr float PLAYER_RADIUS = 0.35f, PLAYER_HEIGHT = 1.8f;

// The surface's pull (a wheel's rolling resistance is set by its weight
// where it's built: near enough anywhere it rolls).
constexpr float SURFACE_G = 9.81f;

// An entity's turn as a rotation matrix: R[r][c] is m[c * 4 + r].
core::matrix4 turnOf(v3f rotation)
{
	core::matrix4 m = piece::frame(v3f(0), rotation);
	return m;
}

// The smooth cylinder a hinged group rolls as, if it's round about its axle.
std::optional<physics::Round> roundAbout(const std::vector<physics::Box> &boxes, v3f anchor, v3f axis)
{
	const int k = axis.X != 0 ? 0 : axis.Y != 0 ? 1 : 2, u = (k + 1) % 3, v = (k + 2) % 3;
	float reach[ROUND_BINS] = {};
	float lo = std::numeric_limits<float>::infinity(), hi = -lo;
	for (const physics::Box &b : boxes) {
		lo = std::min(lo, b.box.MinEdge[k]);
		hi = std::max(hi, b.box.MaxEdge[k]);
		const float u0 = b.box.MinEdge[u] - anchor[u], u1 = b.box.MaxEdge[u] - anchor[u];
		const float v0 = b.box.MinEdge[v] - anchor[v], v1 = b.box.MaxEdge[v] - anchor[v];
		for (int i = 0; i < ROUND_BINS; i++) {
			// how far a ray out from the axle this way is still in the box
			const float a = (i + 0.5f) * 2 * core::PI / ROUND_BINS;
			const float d[2] = { std::cos(a), std::sin(a) }, from[2] = { u0, v0 }, to[2] = { u1, v1 };
			float in = -std::numeric_limits<float>::infinity(), out = -in;
			bool miss = false;
			for (int j = 0; j < 2; j++) {
				if (std::fabs(d[j]) < 1e-6f) {
					miss |= from[j] > 0 || to[j] < 0;
					continue;
				}
				const float t1 = from[j] / d[j], t2 = to[j] / d[j];
				in = std::max(in, std::min(t1, t2));
				out = std::min(out, std::max(t1, t2));
			}
			if (!miss && out >= std::max(in, 0.f))
				reach[i] = std::max(reach[i], out);
		}
	}
	float least = reach[0], most = reach[0], sum = 0;
	for (float r : reach) {
		least = std::min(least, r);
		most = std::max(most, r);
		sum += r;
	}
	if (!(least > 0) || least < ROUND * most)
		return std::nullopt;
	v3f centre = anchor;
	centre[k] = (lo + hi) / 2;
	return physics::Round{ centre, axis, sum / ROUND_BINS, (hi - lo) / 2 };
}

} // namespace

physics::Quat quatOf(v3f rotation)
{
	const core::matrix4 m = turnOf(rotation);
	auto R = [&](int r, int c) { return m[c * 4 + r]; };
	// the usual matrix to quaternion, by its largest diagonal
	physics::Quat q;
	const float t = R(0, 0) + R(1, 1) + R(2, 2);
	if (t > 0) {
		const float s = std::sqrt(t + 1) * 2;
		q = { (R(2, 1) - R(1, 2)) / s, (R(0, 2) - R(2, 0)) / s, (R(1, 0) - R(0, 1)) / s, s / 4 };
	} else if (R(0, 0) > R(1, 1) && R(0, 0) > R(2, 2)) {
		const float s = std::sqrt(1 + R(0, 0) - R(1, 1) - R(2, 2)) * 2;
		q = { s / 4, (R(0, 1) + R(1, 0)) / s, (R(0, 2) + R(2, 0)) / s, (R(2, 1) - R(1, 2)) / s };
	} else if (R(1, 1) > R(2, 2)) {
		const float s = std::sqrt(1 + R(1, 1) - R(0, 0) - R(2, 2)) * 2;
		q = { (R(0, 1) + R(1, 0)) / s, s / 4, (R(1, 2) + R(2, 1)) / s, (R(0, 2) - R(2, 0)) / s };
	} else {
		const float s = std::sqrt(1 + R(2, 2) - R(0, 0) - R(1, 1)) * 2;
		q = { (R(0, 2) + R(2, 0)) / s, (R(1, 2) + R(2, 1)) / s, s / 4, (R(1, 0) - R(0, 1)) / s };
	}
	return q;
}

v3f rotationOf(const physics::Quat &q)
{
	const float x = q.x, y = q.y, z = q.z, w = q.w;
	core::matrix4 m;
	auto set = [&](int r, int c, float v) { m[c * 4 + r] = v; };
	set(0, 0, 1 - 2 * (y * y + z * z));
	set(0, 1, 2 * (x * y - z * w));
	set(0, 2, 2 * (x * z + y * w));
	set(1, 0, 2 * (x * y + z * w));
	set(1, 1, 1 - 2 * (x * x + z * z));
	set(1, 2, 2 * (y * z - x * w));
	set(2, 0, 2 * (x * z - y * w));
	set(2, 1, 2 * (y * z + x * w));
	set(2, 2, 1 - 2 * (x * x + y * y));
	// piece::frame sets the matrix from the rotation negated
	const v3f r = -getPitchYawRoll(m);
	return v3f(wrapDegrees_0_360(r.X), wrapDegrees_0_360(r.Y), wrapDegrees_0_360(r.Z));
}

void Materials::set(content_t c, const Material &m)
{
	if (!(m.density > 0) || !std::isfinite(m.density) || !(m.friction >= 0) || !std::isfinite(m.friction))
		throw std::invalid_argument("a material's density is more than 0 and its friction 0 or more");
	if (m_by_content.size() <= c)
		m_by_content.resize(c + 1);
	m_by_content[c] = m;
}

const Material *Materials::get(content_t c) const
{
	return c < m_by_content.size() && m_by_content[c] ? &*m_by_content[c] : nullptr;
}

std::vector<std::pair<content_t, Material>> Materials::all() const
{
	std::vector<std::pair<content_t, Material>> out;
	for (size_t c = 0; c < m_by_content.size(); c++)
		if (m_by_content[c])
			out.push_back({ (content_t)c, *m_by_content[c] });
	return out;
}

physics::Field fieldOf(const orbit::Field &field)
{
	return [&field](const v3d &at, const v3f &velocity) {
		if (const std::optional<orbit::Field::Here> h = field.at(at, velocity))
			return physics::Here{ h->gravity, h->air };
		// outside every grid (a seam between tiles, for a moment): straight
		// down at the surface's pull, no air
		return physics::Here{ v3f(0, -SURFACE_G, 0), 0 };
	};
}

Motion::Motion(ServerEnvironment &env, IGameDef *gamedef, const Materials &materials, const orbit::Field &field) :
	m_env(env), m_gamedef(gamedef), m_materials(materials)
{
	m_world = std::make_unique<physics::World>(physics::World::Opts{ fieldOf(field),
			[this](v3s16 block, std::vector<aabb3f> &boxes) { return ground(block, boxes); } });
}

Motion::~Motion() = default;

physics::Assembly compile(const piece::Machine &m, const Materials &materials, IGameDef *gamedef)
{
	const NodeDefManager *ndef = gamedef->ndef();
	physics::Assembly a;
	const std::vector<std::vector<u64>> groups = m.rigid();
	std::map<u64, size_t> group_of;
	float mass = 0;
	for (size_t gi = 0; gi < groups.size(); gi++) {
		physics::Body body;
		for (u64 id : groups[gi]) {
			group_of[id] = gi;
			const piece::Machine::Part &p = *m.part(id);
			const core::matrix4 at = p.pose.matrix();
			std::vector<aabb3f> boxes;
			p.object.built().each([&](v3s16 n, MapNode node, const shape::Shape *s) {
				const Material *mat = materials.get(node.getContent());
				if (!mat)
					throw std::runtime_error(ndef->get(node).name + " has no physics material");
				boxes.clear();
				if (s)
					s->boxes(boxes);
				else
					boxes.push_back(aabb3f(v3f(-BS / 2), v3f(BS / 2)));
				for (aabb3f b : boxes) {
					b.MinEdge += intToFloat(n, BS);
					b.MaxEdge += intToFloat(n, BS);
					at.transformBoxEx(b);
					b.MinEdge /= BS;
					b.MaxEdge /= BS;
					body.boxes.push_back({ b, mat->density, mat->friction });
					mass += b.getVolume() * mat->density;
				}
			});
		}
		// a group of nothing built (all ghost) is nothing to move: it's left out
		if (body.boxes.empty()) {
			if (gi == 0)
				throw std::runtime_error("nothing of it is built");
			continue;
		}
		a.bodies.push_back(std::move(body));
	}
	// bodies left out shift the rest down
	std::vector<std::optional<size_t>> body_of(groups.size());
	for (size_t gi = 0, b = 0; gi < groups.size(); gi++) {
		bool built = false;
		for (u64 id : groups[gi])
			built |= !m.part(id)->object.built().empty();
		if (built)
			body_of[gi] = b++;
	}
	size_t hinges = 0;
	for (const piece::Machine::Link &l : m.links())
		hinges += piece::JOINTS[size_t(l.joint)].turns && group_of.at(l.a) != group_of.at(l.b);
	for (const piece::Machine::Link &l : m.links()) {
		if (!piece::JOINTS[size_t(l.joint)].turns)
			continue;
		const std::optional<size_t> ba = body_of[group_of.at(l.a)], bb = body_of[group_of.at(l.b)];
		if (!ba || !bb || *ba == *bb)
			continue;
		// sixteenths from the grid's corner (node (0, 0, 0)'s least) to nodes from its centre
		const v3f anchor = l.anchor / 16.f - v3f(0.5f);
		const v3f axis = v3f::from(l.axis);
		// the part that turns is the one that isn't the machine's root
		const size_t turns = *bb != 0 ? *bb : *ba;
		physics::Body &turning = a.bodies[turns];
		if (!turning.round)
			turning.round = roundAbout(turning.boxes, anchor, axis);
		const float radius = turning.round ? turning.round->radius : 0.5f;
		const float friction = ROLLING * mass / hinges * SURFACE_G * radius;
		const float limit = piece::JOINTS[size_t(l.joint)].limit * core::DEGTORAD;
		// a round part turning freely on a level axle off the root is a
		// wheel (sprint 66), if it's on the first wheel's axis
		const bool wheel = turning.round && limit == 0 && (*ba == 0 || *bb == 0) && axis.Y == 0
				&& (a.wheels.empty() || std::fabs(a.wheels[0].axis.dotProduct(axis)) > 0.99f);
		if (wheel)
			a.wheels.push_back({ turns, turning.round->centre, axis, turning.round->radius, turning.round->half_width, friction });
		else
			a.hinges.push_back({ *ba, *bb, anchor, axis, friction, limit });
	}
	// a wheel something else hangs off by a hinge turns as a hinge, not a
	// wheel (its body follows its wheel: nothing can hang off that)
	for (auto w = a.wheels.begin(); w != a.wheels.end();) {
		const bool hung = std::any_of(a.hinges.begin(), a.hinges.end(),
				[&](const physics::Hinge &h) { return h.a == w->body || h.b == w->body; });
		if (!hung) {
			++w;
			continue;
		}
		const physics::Body &b = a.bodies[w->body];
		a.hinges.push_back({ 0, w->body, b.round->centre, w->axis, w->friction, 0 });
		w = a.wheels.erase(w);
	}
	return a;
}

bool groundBoxes(Map &map, const NodeDefManager *ndef, v3s16 block, std::vector<aabb3f> &boxes)
{
	MapBlock *b = map.getBlockNoCreateNoEx(block);
	if (!b)
		return false;
	std::vector<aabb3f> nb;
	const v3s16 base = block * MAP_BLOCKSIZE;
	// a plain whole block: inside the ground, it's left out
	auto whole = [&](v3s16 rel) {
		const MapNode n = b->getNodeNoCheck(rel);
		const ContentFeatures &f = ndef->get(n);
		std::vector<aabb3f> shaped;
		return f.walkable && f.drawtype == NDT_NORMAL && !map.getShapedBoxes(base + rel, shaped);
	};
	for (s16 x = 0; x < MAP_BLOCKSIZE; x++)
		for (s16 y = 0; y < MAP_BLOCKSIZE; y++)
			for (s16 z = 0; z < MAP_BLOCKSIZE; z++) {
				const v3s16 rel(x, y, z);
				const MapNode n = b->getNodeNoCheck(rel);
				if (n.getContent() == CONTENT_IGNORE)
					return false;
				if (!ndef->get(n).walkable)
					continue;
				const bool inner = x > 0 && y > 0 && z > 0 && x < MAP_BLOCKSIZE - 1 && y < MAP_BLOCKSIZE - 1
						&& z < MAP_BLOCKSIZE - 1;
				if (inner && whole(rel) && whole(rel + v3s16(1, 0, 0)) && whole(rel - v3s16(1, 0, 0))
						&& whole(rel + v3s16(0, 1, 0)) && whole(rel - v3s16(0, 1, 0))
						&& whole(rel + v3s16(0, 0, 1)) && whole(rel - v3s16(0, 0, 1)))
					continue;
				const v3s16 p = base + rel;
				nb.clear();
				if (!map.getShapedBoxes(p, nb))
					n.getCollisionBoxes(ndef, &nb, n.getNeighbors(p, &map));
				for (aabb3f box : nb) {
					box.MinEdge = box.MinEdge / BS + v3f::from(rel);
					box.MaxEdge = box.MaxEdge / BS + v3f::from(rel);
					if (box.getVolume() > 0)
						boxes.push_back(box);
				}
			}
	return true;
}

bool Motion::ground(v3s16 block, std::vector<aabb3f> &boxes)
{
	return groundBoxes(m_env.getMap(), m_gamedef->ndef(), block, boxes);
}

Motion::Tracked *Motion::tracked(u16 object)
{
	auto it = m_tracked.find(object);
	return it == m_tracked.end() ? nullptr : &it->second;
}

void Motion::step(float dtime)
{
	std::vector<ServerActiveObject *> objs;
	m_env.getObjectsInArea(objs, aabb3f(v3f(-1e9f), v3f(1e9f)), [](ServerActiveObject *o) {
		auto *e = dynamic_cast<LuaEntitySAO *>(o);
		return e && !o->isGone() && e->accessObjectProperties()->visual == OBJECTVISUAL_PIECE
				&& !e->accessObjectProperties()->piece.empty();
	});
	std::map<u16, LuaEntitySAO *> seen;
	for (ServerActiveObject *o : objs) {
		auto *e = static_cast<LuaEntitySAO *>(o);
		const std::string &text = e->accessObjectProperties()->piece;
		const size_t hash = std::hash<std::string>{}(text);
		const u16 id = o->getId();
		auto broken = m_broken.find(id);
		if (broken != m_broken.end() && broken->second == hash)
			continue;
		Tracked *t = tracked(id);
		if (t && t->text == hash) {
			seen[id] = e;
			continue;
		}
		try {
			piece::Machine m(m_gamedef);
			m.read(text);
			const physics::Assembly a = compile(m, m_materials, m_gamedef);
			if (t) {
				m_world->rebuild(t->id, a);
				t->text = hash;
			} else {
				const physics::Pose at{ v3d::from(e->getBasePosition() / BS), quatOf(e->getRotation()) };
				m_tracked[id] = { m_world->add(a, at, e->getVelocity() / BS), hash, "" };
				if (m_held.count(id))
					m_world->hold(m_tracked[id].id, true);
				if (auto c = m_carried.find(id); c != m_carried.end())
					m_world->carry(m_tracked[id].id, c->second);
				if (auto g = m_grabbed.find(id); g != m_grabbed.end())
					for (const auto &[who, grab] : g->second)
						grip(id, who, grab, true);
				e->setDriven(true);
			}
			m_broken.erase(id);
			seen[id] = e;
		} catch (const std::exception &ex) {
			errorstream << "physics: machine object " << id << " can't move: " << ex.what() << std::endl;
			m_broken[id] = hash;
			if (t) {
				m_world->remove(t->id);
				m_tracked.erase(id);
			}
			e->setDriven(false);
		}
	}
	for (auto it = m_tracked.begin(); it != m_tracked.end();) {
		if (!seen.count(it->first)) {
			m_world->remove(it->second.id);
			it = m_tracked.erase(it);
		} else {
			++it;
		}
	}
	for (auto it = m_broken.begin(); it != m_broken.end();)
		it = m_env.getActiveObject(it->first) ? std::next(it) : m_broken.erase(it);
	for (auto it = m_held.begin(); it != m_held.end();)
		it = m_env.getActiveObject(*it) ? std::next(it) : m_held.erase(it);
	for (auto it = m_carried.begin(); it != m_carried.end();)
		it = m_env.getActiveObject(it->first) ? std::next(it) : m_carried.erase(it);
	// what's gone isn't grabbed, and who's gone grabs nothing
	for (auto it = m_grabbed.begin(); it != m_grabbed.end();) {
		for (auto g = it->second.begin(); g != it->second.end();) {
			if (m_env.getActiveObject(g->first)) {
				++g;
				continue;
			}
			if (Tracked *t = tracked(it->first))
				m_world->release(t->id, g->first);
			g = it->second.erase(g);
		}
		it = m_env.getActiveObject(it->first) && !it->second.empty() ? std::next(it) : m_grabbed.erase(it);
	}

	// by name: the same order every step, whoever joined when
	std::vector<std::pair<std::string, physics::Capsule>> named;
	for (RemotePlayer *p : m_env.getPlayers())
		if (PlayerSAO *sao = p->getPlayerSAO())
			named.push_back({ p->getName(), { v3d::from(sao->getBasePosition() / BS), PLAYER_RADIUS, PLAYER_HEIGHT } });
	std::sort(named.begin(), named.end(), [](const auto &a, const auto &b) { return a.first < b.first; });
	std::vector<physics::Capsule> players;
	for (const auto &[n, c] : named)
		players.push_back(c);
	m_world->players(players);

	m_world->step(dtime);

	for (auto &[id, t] : m_tracked) {
		LuaEntitySAO *e = seen.at(id);
		const physics::Pose p = m_world->pose(t.id);
		e->setBasePosition(v3f::from(p.position * BS));
		e->setRotation(rotationOf(p.rotation));
		e->setVelocity(m_world->velocity(t.id) * BS);
		std::ostringstream os(std::ios::binary);
		const std::vector<physics::Relative> rel = m_world->relatives(t.id);
		writeU16(os, rel.size());
		for (const physics::Relative &r : rel) {
			writeV3F32(os, r.offset * BS);
			writeF32(os, r.rotation.x);
			writeF32(os, r.rotation.y);
			writeF32(os, r.rotation.z);
			writeF32(os, r.rotation.w);
		}
		if (!rel.empty() && os.str() != t.parts_sent) {
			t.parts_sent = os.str();
			e->sendParts(t.parts_sent);
		}
	}
}

void Motion::mapEdited(const MapEditEvent &e)
{
	for (const v3s16 &b : e.modified_blocks)
		m_world->groundChanged(b);
}

void Motion::hold(LuaEntitySAO &obj, bool held)
{
	if (held)
		carry(obj, std::nullopt);
	if (held)
		m_held.insert(obj.getId());
	else
		m_held.erase(obj.getId());
	if (Tracked *t = tracked(obj.getId()))
		m_world->hold(t->id, held);
}

bool Motion::held(const LuaEntitySAO &obj) const
{
	return m_held.count(obj.getId()) != 0;
}

void Motion::carry(LuaEntitySAO &obj, std::optional<float> strength)
{
	if (strength)
		m_carried[obj.getId()] = *strength;
	else
		m_carried.erase(obj.getId());
	if (Tracked *t = tracked(obj.getId()))
		m_world->carry(t->id, strength);
}

bool Motion::carried(const LuaEntitySAO &obj) const
{
	return m_carried.count(obj.getId()) != 0;
}

void Motion::pose(LuaEntitySAO &obj, v3f position, v3f rotation)
{
	const physics::Pose to{ v3d::from(position / BS), quatOf(rotation) };
	if (held(obj)) {
		if (Tracked *t = tracked(obj.getId()))
			m_world->moveTo(t->id, to);
	} else if (carried(obj)) {
		if (Tracked *t = tracked(obj.getId()))
			m_world->pullTo(t->id, to);
	} else {
		throw std::logic_error("Motion::pose: object " + std::to_string(obj.getId()) + " isn't held or carried");
	}
}

void Motion::grab(LuaEntitySAO &obj, u16 who, std::optional<Grab> grab)
{
	const u16 id = obj.getId();
	if (!grab) {
		if (auto it = m_grabbed.find(id); it != m_grabbed.end())
			it->second.erase(who);
		if (Tracked *t = tracked(id))
			m_world->release(t->id, who);
		return;
	}
	const bool fresh = !grabbing(obj, who);
	// a grip's point is where it was first taken: later, only the hand moves
	if (!fresh)
		grab->point = m_grabbed[id][who].point;
	m_grabbed[id][who] = *grab;
	grip(id, who, *grab, fresh);
}

bool Motion::grabbing(const LuaEntitySAO &obj, u16 who) const
{
	auto it = m_grabbed.find(obj.getId());
	return it != m_grabbed.end() && it->second.count(who);
}

void Motion::grip(u16 object, u16 who, const Grab &g, bool fresh)
{
	Tracked *t = tracked(object);
	if (!t)
		return; // gripped when it's built
	if (fresh)
		m_world->grab(t->id, who, g.point / BS, g.strength);
	m_world->grabTo(t->id, who, v3d::from(g.hand / BS));
}

std::optional<v3f> Motion::velocity(u16 object) const
{
	auto it = m_tracked.find(object);
	return it == m_tracked.end() ? std::nullopt : std::optional<v3f>(m_world->velocity(it->second.id) * BS);
}

std::optional<float> Motion::mass(u16 object) const
{
	auto it = m_tracked.find(object);
	return it == m_tracked.end() ? std::nullopt : std::optional<float>(m_world->mass(it->second.id));
}

void Motion::impulse(u16 object, v3f impulse, std::optional<v3f> from)
{
	Tracked *t = tracked(object);
	if (!t)
		throw std::invalid_argument("not a machine physics moves");
	m_world->impulse(t->id, impulse / BS, from ? std::optional<v3d>(v3d::from(*from / BS)) : std::nullopt);
}

} // namespace workshop
