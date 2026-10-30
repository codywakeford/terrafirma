// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "workshop.h"

#include "gamedef.h"
#include "nodedef.h"
#include "piece/frame.h"
#include "remoteplayer.h"
#include "server/luaentity_sao.h"
#include "server/player_sao.h"
#include "serverenvironment.h"
#include "settings.h"
#include "log.h"
#include <algorithm>
#include <cmath>
#include <map>

namespace workshop {

namespace {

constexpr float AHEAD = 2.5f; // nodes in front a new one's held

const std::string &name(PlayerSAO &who)
{
	return who.getPlayer()->getName();
}

piece::Machine::Part &partOf(piece::Machine &m, u64 part)
{
	piece::Machine::Part *p = m.part(part);
	if (!p)
		throw Refused("That part's gone");
	return *p;
}

} // namespace

Workshop::Workshop(const Opts &opts) :
	m_env(opts.env), m_gamedef(opts.gamedef), m_entities(opts.env, opts.gamedef), m_library(opts.world_path),
	m_motion(opts.env, opts.gamedef, opts.materials)
{
}

void Workshop::stroke(PlayerSAO &who, u16 id, u64 part, const sculpt::Stroke &s, std::optional<piece::Joint> apart)
{
	if (apart && s.act != sculpt::Act::Add)
		throw Refused("Only what's added makes a part");
	// on the world, or on a machine's part's design in its own grid
	LuaEntitySAO *obj = id ? &m_entities.object(id) : nullptr;
	std::optional<piece::Machine> m;
	v3f there = intToFloat(s.at.node, BS);
	if (obj) {
		m = m_entities.read(*obj);
		there = obj->getBasePosition();
	}
	piece::Machine::Part *p = m ? &partOf(*m, part) : nullptr;
	within(who, there, s.size / 16.f + (m ? piece::SIDE * 2 : 0));

	// adding past a part's grid (its back faces: node 0's least), the grid's
	// moved to make room, and the stroke with it
	sculpt::Stroke stroke = s;
	if (p && s.act == sculpt::Act::Add && !apart) {
		const std::vector<v3s32> cells = s.cells();
		v3s32 lo(cells[0].X >> 4, cells[0].Y >> 4, cells[0].Z >> 4), hi = lo;
		for (const v3s32 &c : cells)
			for (int a = 0; a < 3; a++) {
				lo[a] = std::min(lo[a], c[a] >> 4);
				hi[a] = std::max(hi[a], c[a] >> 4);
			}
		v3s16 by;
		try {
			by = m->makeRoom(part, lo, hi);
		} catch (const BaseException &) {
			throw Refused("A part's at most 16 blocks a side: make another part");
		}
		if (by != v3s16(0, 0, 0)) {
			stroke.at.node += by;
			if (stroke.to)
				stroke.to->node += by;
			if (stroke.mirror)
				stroke.mirror->twice += 2 * by[stroke.mirror->axis] * shape::STEPS;
			m_history.shift(m->id(), part, by);
		}
	}
	Map &map = p ? p->object.design().map() : m_env.getMap();
	bool valid;
	const MapNode pointed = map.getNode(stroke.at.node, &valid);
	if (!valid)
		throw Refused("That's not loaded yet");

	// what's placed or painted: the block held, else the material pointed at
	content_t material = CONTENT_IGNORE;
	if (s.act != sculpt::Act::Cut) {
		material = held(who);
		if (material == CONTENT_IGNORE && m_gamedef->ndef()->get(pointed).drawtype == NDT_NORMAL)
			material = pointed.getContent();
		if (material == CONTENT_IGNORE)
			throw Refused("Nothing to place: hold a block, or point at one");
	}
	if (apart && !m) {
		// drawn on the world: a new object, of what's drawn, where it is
		const std::vector<v3s32> cells = s.cells();
		v3s32 least(cells[0].X >> 4, cells[0].Y >> 4, cells[0].Z >> 4);
		for (const v3s32 &c : cells)
			for (int a = 0; a < 3; a++)
				least[a] = std::min(least[a], c[a] >> 4);
		piece::Object o(m_gamedef);
		std::map<v3s16, shape::Shape> nodes;
		for (const v3s32 &c : cells) {
			const v3s32 l = c - least * 16;
			const v3s16 n(l.X >> 4, l.Y >> 4, l.Z >> 4);
			if (n.X >= piece::SIDE || n.Y >= piece::SIDE || n.Z >= piece::SIDE)
				throw Refused("That's bigger than an object can start");
			nodes[n].set(v3s16(l.X & 15, l.Y & 15, l.Z & 15), true);
		}
		for (const auto &[n, cut] : nodes) {
			o.design().map().setNode(n, MapNode(material));
			o.design().map().setShape(n, cut);
		}
		o.build();
		piece::Machine made(m_gamedef);
		made.add({ std::move(o), piece::Pose{ v3s32(0), 0 } });
		m_entities.spawn(intToFloat(v3s16(least.X, least.Y, least.Z), BS), v3f(0), made); // physics lets it fall
		actionstream << "workshop: " << name(who) << " drew machine " << made.id() << " on the world" << std::endl;
		return;
	}
	if (apart) {
		// its own part, and its mirror's another (a pair of wheels)
		const piece::Joint joint = *apart;
		const piece::Machine::Face face{ part, s.at, s.normal };
		sculpt::Stroke one = s;
		one.mirror.reset();
		m->grow(face, one.cells(), material, joint);
		if (s.mirror) {
			std::vector<v3s32> other = one.cells();
			for (v3s32 &c : other)
				c[s.mirror->axis] = s.mirror->twice - 1 - c[s.mirror->axis];
			sculpt::Stroke across = one;
			across.normal[s.mirror->axis] = -across.normal[s.mirror->axis];
			const v3s32 at = sculpt::world(s.at);
			v3s32 mirrored = at;
			mirrored[s.mirror->axis] = s.mirror->twice - 1 - at[s.mirror->axis];
			m->grow({ part, sculpt::local(mirrored), across.normal }, other, material, joint);
		}
		m_entities.store(*obj, *m);
		actionstream << "workshop: " << name(who) << " grew a part (" << piece::JOINTS[size_t(joint)].id
				<< ") on machine " << m->id() << std::endl;
		return;
	}
	sculpt::Edit edit = sculpt::Sculptor(map).apply(stroke, material,
			p ? std::optional<VoxelArea>(piece::Piece::area()) : std::nullopt);
	if (m) {
		m->build(); // creative: nothing's left to make
		m_entities.store(*obj, *m);
	}
	m_history.record(name(who), { m ? m->id() : 0, part, std::move(edit) });
	actionstream << "workshop: " << name(who) << " " << sculpt::KINDS[size_t(s.tool)].id << " stroke on "
			<< (m ? "machine " + std::to_string(m->id()) : std::string("the world")) << std::endl;
}

void Workshop::undo(PlayerSAO &who)
{
	const std::optional<sculpt::History::Entry> last = m_history.pop(name(who));
	if (!last)
		throw Refused("Nothing to undo");
	if (!last->machine) {
		last->edit.restore(m_env.getMap());
		return;
	}
	LuaEntitySAO *obj = m_entities.find(who, last->machine);
	if (!obj)
		throw Refused("That object is gone");
	piece::Machine m = m_entities.read(*obj);
	last->edit.restore(partOf(m, last->part).object.design().map());
	m.build();
	m_entities.store(*obj, m);
}

u16 Workshop::start(PlayerSAO &who)
{
	const content_t material = held(who);
	if (material == CONTENT_IGNORE)
		throw Refused("Hold a block to start an object from");
	const f32 yaw = who.getRotation().Y * core::DEGTORAD;
	const v3f at = who.getEyePosition() + v3f(-std::sin(yaw), 0, std::cos(yaw)) * AHEAD * BS - v3f(0, 0.5f * BS, 0);
	bool valid;
	if (!m_gamedef->ndef()->get(m_env.getMap().getNode(floatToInt(at, BS), &valid)).buildable_to || !valid)
		throw Refused("There's no room in front of you");
	piece::Machine m(m_gamedef);
	piece::Object o(m_gamedef);
	o.design().map().setNode({ 0, 0, 0 }, MapNode(material));
	o.build();
	m.add({ std::move(o), piece::Pose{ v3s32(0), 0 } });
	LuaEntitySAO &obj = m_entities.spawn(at, v3f(0, who.getRotation().Y, 0), m);
	m_motion.hold(obj, true); // made held: it floats in front of them till it's let go
	actionstream << "workshop: " << name(who) << " made machine " << m.id() << std::endl;
	return obj.getId();
}

void Workshop::hold(PlayerSAO &who, u16 id, bool held)
{
	LuaEntitySAO &obj = m_entities.object(id);
	within(who, obj.getBasePosition(), piece::SIDE * 2);
	obj.setVelocity(v3f(0));
	m_motion.hold(obj, held);
	actionstream << "workshop: " << name(who) << (held ? " holds" : " lets go of") << " object " << id << std::endl;
}

void Workshop::pose(PlayerSAO &who, u16 id, v3f position, v3f rotation)
{
	auto finite = [](v3f v) { return std::isfinite(v.X) && std::isfinite(v.Y) && std::isfinite(v.Z); };
	if (!finite(position) || !finite(rotation))
		throw Refused("That's not a pose");
	LuaEntitySAO &obj = m_entities.object(id);
	// the holder sends poses ~20 times a second: one sent before they let
	// go can arrive after; it's moot, not an error (the physics has it now)
	if (!m_motion.held(obj))
		return;
	within(who, position, piece::SIDE * 2);
	obj.setVelocity(v3f(0));
	obj.moveTo(position, true);
	obj.setRotation(v3f(wrapDegrees_0_360(rotation.X), wrapDegrees_0_360(rotation.Y), wrapDegrees_0_360(rotation.Z)));
	m_motion.pose(obj, position, obj.getRotation());
}

void Workshop::hollow(PlayerSAO &who, u16 id, u64 part, int thickness)
{
	if (thickness < 1 || thickness > sculpt::MAX_SIZE)
		throw Refused("A wall's 1 to 64 sixteenths");
	LuaEntitySAO &obj = m_entities.object(id);
	within(who, obj.getBasePosition(), piece::SIDE * 2);
	piece::Machine m = m_entities.read(obj);
	sculpt::Edit edit = sculpt::Sculptor(partOf(m, part).object.design().map()).hollow(piece::Piece::area(), thickness);
	if (edit.was.empty())
		throw Refused("Nothing's thicker than that to hollow");
	m.build();
	m_entities.store(obj, m);
	m_history.record(name(who), { m.id(), part, std::move(edit) });
}

void Workshop::relink(PlayerSAO &who, u16 id, u64 a, u64 b, std::optional<piece::Joint> joint,
		const std::optional<piece::Machine::Face> &at)
{
	LuaEntitySAO &obj = m_entities.object(id);
	within(who, obj.getBasePosition(), piece::SIDE * 2);
	piece::Machine m = m_entities.read(obj);
	partOf(m, a);
	partOf(m, b);
	if (a == b)
		throw Refused("Join a part to another, not to itself");
	if (joint)
		m.relink(a, b, *joint, at);
	else
		m.unlink(a, b);
	m_entities.store(obj, m);
	actionstream << "workshop: " << name(who) << " set a joint on machine " << m.id() << " to "
			<< (joint ? piece::JOINTS[size_t(*joint)].id : "none") << std::endl;
}

void Workshop::join(PlayerSAO &who, u16 a, const piece::Machine::Face &on_a, u16 b, const piece::Machine::Face &on_b,
		piece::Joint joint)
{
	if (a == b)
		throw Refused("Join two things, not one to itself");
	LuaEntitySAO &into = m_entities.object(a), &from = m_entities.object(b);
	within(who, into.getBasePosition(), piece::SIDE * 2);
	within(who, from.getBasePosition(), piece::SIDE * 2);
	piece::Machine m = m_entities.read(into), other = m_entities.read(from);
	partOf(m, on_a.part);
	partOf(other, on_b.part);
	m.join(on_a, std::move(other), on_b, joint);
	m_entities.store(into, m);
	from.markForRemoval();
	actionstream << "workshop: " << name(who) << " joined object " << b << " onto " << a << " ("
			<< piece::JOINTS[size_t(joint)].id << ")" << std::endl;
}

void Workshop::detach(PlayerSAO &who, u16 id, u64 part)
{
	LuaEntitySAO &obj = m_entities.object(id);
	within(who, obj.getBasePosition(), piece::SIDE * 2);
	piece::Machine m = m_entities.read(obj);
	if (m.parts().size() < 2)
		throw Refused("It's in one part already");
	piece::Machine::Part p = m.take(part);
	// where it was: the machine's frame, then the part's in it
	const core::matrix4 world = piece::frame(obj.getBasePosition(), obj.getRotation()) * p.pose.matrix();
	p.pose = piece::Pose{ v3s32(0), 0 };
	piece::Machine alone(m_gamedef);
	alone.add(std::move(p));
	m_entities.store(obj, m);
	m_entities.spawn(world.getTranslation(), -getPitchYawRoll(world), alone); // physics lets it fall
}

void Workshop::socket(PlayerSAO &who, u16 id, const piece::Machine::Face &face, piece::Joint joint,
		const std::optional<sculpt::Stroke::Mirror> &mirror)
{
	LuaEntitySAO &obj = m_entities.object(id);
	within(who, obj.getBasePosition(), piece::SIDE * 2);
	piece::Machine m = m_entities.read(obj);
	partOf(m, face.part);
	m.socket(face, joint);
	if (mirror)
		m.socket(m.mirrored(face, mirror->axis, mirror->twice), joint);
	m_entities.store(obj, m);
	actionstream << "workshop: " << name(who) << " made a " << piece::JOINTS[size_t(joint)].id
			<< " socket on machine " << m.id() << std::endl;
}

void Workshop::unsocket(PlayerSAO &who, u16 id, u64 socket)
{
	LuaEntitySAO &obj = m_entities.object(id);
	within(who, obj.getBasePosition(), piece::SIDE * 2);
	piece::Machine m = m_entities.read(obj);
	m.unsocket(socket);
	m_entities.store(obj, m);
}

void Workshop::plug(PlayerSAO &who, u16 a, u64 socket, u16 b, const piece::Machine::Face &on_b)
{
	if (a == b)
		throw Refused("Plug one thing into another, not into itself");
	LuaEntitySAO &into = m_entities.object(a), &from = m_entities.object(b);
	within(who, into.getBasePosition(), piece::SIDE * 2);
	within(who, from.getBasePosition(), piece::SIDE * 2);
	piece::Machine m = m_entities.read(into), other = m_entities.read(from);
	partOf(other, on_b.part);
	if (std::none_of(m.sockets().begin(), m.sockets().end(), [&](const piece::Machine::Socket &k) { return k.id == socket; }))
		throw Refused("That socket's taken, or gone");
	m.plug(socket, std::move(other), on_b);
	m_entities.store(into, m);
	from.markForRemoval();
	actionstream << "workshop: " << name(who) << " plugged object " << b << " into " << a << std::endl;
}

void Workshop::remove(PlayerSAO &who, u16 id, u64 part)
{
	LuaEntitySAO &obj = m_entities.object(id);
	within(who, obj.getBasePosition(), piece::SIDE * 2);
	piece::Machine m = m_entities.read(obj);
	partOf(m, part);
	m.take(part);
	m_entities.store(obj, m); // its last part gone, it is
}

void Workshop::save(PlayerSAO &who, u16 id, const std::string &as)
{
	if (!Library::named(as))
		throw Refused("A blueprint's name is up to 32 letters, digits, spaces, - and _");
	const piece::Machine m = m_entities.read(m_entities.object(id));
	m_library.save(name(who), as, Blueprint{ { { v3s16(0), m.write() } } });
}

void Workshop::place(PlayerSAO &who, const std::string &as, v3s16 at)
{
	within(who, intToFloat(at, BS), 0);
	Blueprint blueprint;
	try {
		blueprint = m_library.load(name(who), as);
	} catch (const std::out_of_range &) {
		throw Refused("You've no blueprint \"" + as + "\"");
	}
	bool valid;
	if (!m_gamedef->ndef()->get(m_env.getMap().getNode(at, &valid)).buildable_to || !valid)
		throw Refused("It's not open there");
	for (const Blueprint::Part &part : blueprint.parts) {
		piece::Machine m(m_gamedef);
		m.read(part.machine);
		m.renew();
		// TEMP: 2026-10-29: built whole, as creative is: in survival it's
		// placed unbuilt, what's left to make, with the survival tools
		m.build();
		m_entities.spawn(intToFloat(at + part.offset, BS), v3f(0), m);
	}
}

void Workshop::within(PlayerSAO &who, v3f there, float extra) const
{
	if (who.getEyePosition().getDistanceFrom(there) > (sculpt::REACH + extra) * BS)
		throw Refused("That's out of reach");
}

content_t Workshop::held(PlayerSAO &who) const
{
	ItemStack selected;
	const ItemStack stack = who.getWieldedItem(&selected);
	content_t id;
	const NodeDefManager *ndef = m_gamedef->ndef();
	if (stack.empty() || !ndef->getId(stack.name, id) || ndef->get(id).drawtype != NDT_NORMAL)
		return CONTENT_IGNORE;
	return id;
}

} // namespace workshop
