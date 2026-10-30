// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "body.h"

#include "client.h"
#include "collision.h"
#include "content_cao.h"
#include "localplayer.h"
#include "log.h"
#include "nodedef.h"
#include "piece/machine.h"
#include "planet/atlas.h"
#include "settings.h"
#include "terragen/planet.h"
#include "workshop/motion.h"
#include <cmath>
#include <functional>

namespace {

// How far round the player machines are in its world (nodes).
constexpr float MIRRORED = 48;
// the player's key for what they grab (one world, one grabber)
constexpr unsigned ME = 1;
// The player as a character: as Luanti's box is wide and tall, a person's
// weight, and pushing as a person can.
constexpr float MASS = 75, PUSH = 600, SLOPE = 50;
// Jumping clears a block (nodes), as Luanti's jump did under its doubled gravity.
constexpr float JUMP = 1.25f;
// A person to the air: a drag coefficient times the area they show (m²).
constexpr float DRAG_AREA = 0.7f;
// Thrusting (nodes/s²): from about a g, doubling each few seconds it's
// held, so a hop is gentle and orbit (a hundred-odd m/s) is a short burn.
constexpr float THRUST = 10, DOUBLING = 4, THRUST_MOST = 160;

} // namespace

Body::Body(Client *client) : m_client(client) {}

Body::~Body() = default;

bool Body::moves(LocalPlayer &p)
{
	PlayerSettings &s = p.getPlayerSettings();
	const bool fly = m_client->checkLocalPrivilege("fly") && s.free_move;
	const bool noclip = m_client->checkLocalPrivilege("noclip") && s.noclip;
	// flying, Ctrl thrusts; it lasts till they land or stop flying
	if (!fly || noclip || p.getParent())
		m_thrusting = false;
	else if (p.control.aux1)
		m_thrusting = true;
	if (m_thrusting)
		return true;
	return !p.getParent() && !fly && !noclip && !p.in_liquid && !p.is_climbing;
}

physics::Here Body::here(const v3d &foot, v3f velocity) const
{
	physics::Here h = m_pull(foot, velocity);
	h.gravity *= m_scale;
	return h;
}

void Body::ensureWorld()
{
	const planet::Atlas *atlas = m_client->getAtlas();
	if (m_world && atlas == m_atlas)
		return;
	// a new planet (or the first): a new field, a new world
	m_atlas = atlas;
	m_world.reset();
	m_field = atlas ? std::make_unique<orbit::Field>(*atlas) : std::make_unique<orbit::Field>(terragen::home());
	m_pull = workshop::fieldOf(*m_field);
	m_mirrors.clear();
	m_me.reset();
	Client *client = m_client;
	m_world = std::make_unique<physics::World>(physics::World::Opts{
			[this](const v3d &at, const v3f &velocity) { return here(at, velocity); },
			[client](v3s16 block, std::vector<aabb3f> &boxes) {
				return workshop::groundBoxes(client->getEnv().getMap(), client->ndef(), block, boxes);
			} });
	if (m_carrying)
		m_carrying.reset(); // its mirror's gone with the world; Game lets it go
	m_grabbing.reset();
}

void Body::nudge(v3f by)
{
	if (!m_me)
		return; // not walking yet: Luanti's movement had it
	const physics::World::CharacterState c = m_world->character(*m_me);
	m_world->place(*m_me, c.foot, c.velocity + by);
	m_now = m_was = m_measured = c.foot;
	m_set = v3f::from(c.foot * BS);
}

void Body::groundChanged(v3s16 block)
{
	if (m_world)
		m_world->groundChanged(block);
}

void Body::mirror(LocalPlayer &p)
{
	std::vector<DistanceSortedActiveObject> near;
	m_client->getEnv().getActiveObjects(p.getPosition(), MIRRORED * BS, near);
	std::map<u16, GenericCAO *> seen;
	for (const DistanceSortedActiveObject &d : near) {
		auto *cao = dynamic_cast<GenericCAO *>(d.obj);
		if (!cao || !cao->getMachine())
			continue;
		const u16 id = cao->getId();
		const size_t text = std::hash<std::string>{}(cao->getProperties().piece);
		auto unweighed = m_unweighed.find(id);
		if (unweighed != m_unweighed.end() && unweighed->second == text)
			continue;
		seen[id] = cao;
		auto it = m_mirrors.find(id);
		if (it != m_mirrors.end() && it->second.text == text)
			continue;
		try {
			const physics::Assembly a = workshop::compile(*cao->getMachine(), m_client->getMaterials(), m_client);
			if (it != m_mirrors.end()) {
				m_world->rebuild(it->second.id, a);
				it->second.text = text;
			} else {
				const physics::Pose at{ v3d::from(cao->getPosition() / BS), workshop::quatOf(cao->getRotation()) };
				const physics::World::Id w = m_world->add(a, at);
				m_world->hold(w, true); // the server's: it goes where it's drawn
				m_mirrors[id] = { w, text };
			}
			m_unweighed.erase(id);
		} catch (const std::runtime_error &e) {
			// the server can't move it either (it says so in its log): here
			// it's no obstacle, as it isn't there
			infostream << "Body: machine " << id << " isn't in the player's world: " << e.what() << std::endl;
			m_unweighed[id] = text;
			seen.erase(id);
			if (it != m_mirrors.end()) {
				m_world->remove(it->second.id);
				m_mirrors.erase(it);
			}
		}
	}
	for (auto it = m_mirrors.begin(); it != m_mirrors.end();) {
		const bool carried = m_carrying && m_carrying->first == it->first;
		const bool grabbed = m_grabbing && *m_grabbing == it->first;
		auto s = seen.find(it->first);
		if (s == seen.end()) {
			m_world->remove(it->second.id);
			if (carried)
				m_carrying.reset();
			if (grabbed)
				m_grabbing.reset();
			it = m_mirrors.erase(it);
			continue;
		}
		if (grabbed) {
			// toward where the server has it, as far on as the time its word
			// took to come; a share each frame, or put there if far off
			constexpr float LATE = 0.1f, SHARE = 0.05f, SNAP = 1.5f; // s; of the gap; nodes
			const GenericCAO::Served &s_ = s->second->served();
			m_world->correct(it->second.id, { v3d::from((s_.position + s_.velocity * LATE) / BS),
					workshop::quatOf(s_.rotation) }, SHARE, SNAP);
		} else if (!carried) // what's carried is this client's to move
			m_world->moveTo(it->second.id, { v3d::from(s->second->getPosition() / BS),
					workshop::quatOf(s->second->getRotation()) });
		++it;
	}
}

void Body::move(LocalPlayer &p, float dtime, std::vector<CollisionInfo> *collisions)
{
	ensureWorld();
	m_scale = p.physics_override.gravity;
	const aabb3f &box = p.getCollisionbox();
	const v3f at = p.getPosition();
	if (!m_me) {
		const float radius = std::max({ -box.MinEdge.X, box.MaxEdge.X, -box.MinEdge.Z, box.MaxEdge.Z }) / BS;
		m_me = m_world->addCharacter({ radius, box.getExtent().Y / BS, MASS, PUSH, SLOPE }, v3d::from(at / BS));
		m_now = m_was = m_measured = v3d::from(at / BS);
		m_set = at;
	}
	if (at.getDistanceFrom(m_set) > 0.001f * BS) {
		// put elsewhere since (by the server, Luanti's movement, a seam's
		// crossing): from there
		m_world->place(*m_me, v3d::from(at / BS), p.getSpeed() / BS);
		m_now = m_was = m_measured = v3d::from(at / BS);
		m_behind = 0;
	}
	// flying through the sky faster than the map arrives: what isn't here
	// yet is air (walking, it's waited for)
	m_world->unknownIsAir(*m_me, m_thrusting);
	mirror(p);
	m_moved = true;

	physics::Gait gait;
	const PlayerControl &c = p.control;
	const PlayerPhysicsOverride &o = p.physics_override;
	gait.walk = (c.sneak ? p.movement_speed_crouch * o.speed_crouch : p.movement_speed_walk * o.speed_walk) / BS;
	gait.run = c.sneak ? 1 : o.speed;
	gait.jump = JUMP * o.jump;
	const physics::Intent intent{ std::cos(c.movement_direction) * c.movement_speed,
		std::sin(c.movement_direction) * c.movement_speed, p.getYaw() * core::DEGTORAD, c.jump };

	m_behind += dtime;
	while (m_behind >= physics::STEP) {
		const physics::World::CharacterState was = m_world->character(*m_me);
		// in the air, its motion carried to where it is now (the grid's axes turn)
		const v3f v = was.on_ground ? was.velocity : m_field->carry(m_measured, was.foot, was.velocity);
		m_measured = was.foot;
		const physics::Here h = here(was.foot, v);
		v3f next;
		if (m_thrusting) {
			next = thrust(p, v, h, (float)physics::STEP);
		} else {
			gait.gravity = h.gravity;
			next = physics::walk(gait, intent, { v, was.on_ground, was.ground_velocity }, (float)physics::STEP);
			if (!was.on_ground) // falling: the air holds it back
				next -= v * (0.5f * h.air * v.getLength() * DRAG_AREA / MASS * (float)physics::STEP);
		}
		if (!was.on_ground) {
			// it goes by its velocity as it is halfway there (the grid's
			// lines curve round the planet), and it's measured there
			m_measured = was.foot + v3d::from(next) * (physics::STEP / 2);
			next = m_field->carry(was.foot, m_measured, next);
		}
		m_world->want(*m_me, next);
		m_world->step(physics::STEP);
		m_behind -= physics::STEP;
		physics::World::CharacterState now = m_world->character(*m_me);
		m_was = m_now;
		m_now = now.foot;
		if (const std::optional<planet::Atlas::Carried> c = m_atlas ?
				m_atlas->carry(was.foot.X, was.foot.Y, was.foot.Z, now.foot.X, now.foot.Y, now.foot.Z) : std::nullopt) {
			// over a seam: into the grid it's in now, the same motion in its
			// axes, the view turned with the ground
			const v3d there(c->x, c->y, c->z);
			m_world->place(*m_me, there, m_field->carry(m_measured, there, now.velocity));
			m_was = m_now = m_measured = there;
			p.turnWithGround(c->angle);
			now = m_world->character(*m_me);
		}
		if (!was.on_ground && now.on_ground) {
			if (m_thrusting) {
				// landed: on their feet, flying off (as a double-tap would)
				m_thrusting = false;
				g_settings->setBool("free_move", false);
			}
			if (collisions) {
				// landed: as a collision with the ground, so a hard one hurts
				CollisionInfo landed;
				landed.type = COLLISION_NODE;
				landed.axis = COLLISION_AXIS_Y;
				landed.node_p = floatToInt(v3f::from(now.foot) * BS - v3f(0, 0.05f * BS, 0), BS);
				landed.old_speed = was.velocity * BS;
				landed.new_speed = now.velocity * BS;
				collisions->push_back(landed);
			}
		}
	}
	push(dtime);
	const physics::World::CharacterState now = m_world->character(*m_me);
	const v3d drawn = m_was + (m_now - m_was) * (m_behind / physics::STEP);
	m_set = v3f::from(drawn * BS);
	p.setPosition(m_set);
	p.setSpeed(now.velocity * BS);
	p.touching_ground = now.on_ground;
}

v3f Body::thrust(LocalPlayer &p, v3f velocity, const physics::Here &h, float dt)
{
	const PlayerControl &c = p.control;
	// where they look (Luanti's: yaw turns +z toward −x, pitch up is negative)
	const float yaw = c.yaw * core::DEGTORAD, pitch = c.pitch * core::DEGTORAD;
	const v3f look(-std::sin(yaw) * std::cos(pitch), -std::sin(pitch), std::cos(yaw) * std::cos(pitch));
	const v3f right(std::cos(yaw), 0, std::sin(yaw));
	v3f ask = look * (std::cos(c.movement_direction) * c.movement_speed) +
			right * (std::sin(c.movement_direction) * c.movement_speed) +
			v3f(0, (c.jump ? 1.0f : 0.0f) - (c.sneak ? 1.0f : 0.0f), 0);
	if (ask.getLengthSQ() > 1)
		ask.normalize();
	const float speed = velocity.getLength();
	const bool pushing = ask.getLengthSQ() > 0 || (m_brake && speed > 0);
	m_held = pushing ? m_held + dt : 0;
	const float push = std::min(THRUST_MOST, THRUST * std::exp2(m_held / DOUBLING));

	const v3f drag = velocity * (-0.5f * h.air * speed * DRAG_AREA / MASS);
	v3f next = velocity + (h.gravity + drag + ask * push) * dt;
	if (m_brake) {
		// against the motion, as hard: to a stop, then holding it there
		const float most = push * dt, left = next.getLength();
		next = left <= most ? v3f(0) : next * ((left - most) / left);
	}
	return next;
}

std::optional<orbit::Elements> Body::orbit() const
{
	if (!m_me)
		return std::nullopt;
	const physics::World::CharacterState c = m_world->character(*m_me);
	return m_field->elements(c.foot, c.velocity);
}

std::optional<std::vector<orbit::Vec>> Body::path(int points) const
{
	if (!m_me)
		return std::nullopt;
	const physics::World::CharacterState c = m_world->character(*m_me);
	return m_field->path(c.foot, c.velocity, points);
}

void Body::stepObjects(LocalPlayer &p, float dtime)
{
	if (m_moved || !m_world) {
		m_moved = false;
		m_idle = 0;
		return;
	}
	mirror(p);
	m_idle += dtime;
	while (m_idle >= physics::STEP) {
		m_world->step(physics::STEP);
		m_idle -= physics::STEP;
	}
}

void Body::push(float dtime)
{
	constexpr float SEND = 0.05f; // s between pushes sent
	for (const auto &[id, impulse] : m_world->pushes(*m_me))
		for (const auto &[object, mirror] : m_mirrors)
			if (mirror.id == id)
				m_pushed[object] += impulse;
	m_push_age += dtime;
	if (m_push_age < SEND)
		return;
	m_push_age = 0;
	for (const auto &[object, impulse] : m_pushed)
		m_client->sendPush(object, impulse);
	m_pushed.clear();
}

void Body::carry(u16 object, float strength)
{
	auto it = m_mirrors.find(object);
	if (!m_world || it == m_mirrors.end())
		return; // not in its world (it can't be weighed): the server's drawing is all there is
	m_world->hold(it->second.id, false);
	m_world->carry(it->second.id, strength);
	m_carrying = { object, strength };
}

void Body::pull(u16 object, v3f position, v3f rotation)
{
	if (!m_carrying || m_carrying->first != object)
		return;
	const physics::World::Id id = m_mirrors.at(object).id;
	m_world->pullTo(id, { v3d::from(position / BS), workshop::quatOf(rotation) });
}

void Body::letGo(u16 object)
{
	if (!m_carrying || m_carrying->first != object)
		return;
	m_carrying.reset();
	auto it = m_mirrors.find(object);
	if (it == m_mirrors.end())
		return;
	m_world->carry(it->second.id, std::nullopt);
	m_world->hold(it->second.id, true); // the server's again: where it's drawn
}

void Body::grab(u16 object, v3f point, float strength)
{
	auto it = m_mirrors.find(object);
	if (!m_world || it == m_mirrors.end())
		return; // not in its world: the server's drawing is all there is
	m_world->hold(it->second.id, false);
	m_world->grab(it->second.id, ME, point / BS, strength);
	m_grabbing = object;
}

void Body::grabTo(u16 object, v3f hand)
{
	if (!m_grabbing || *m_grabbing != object)
		return;
	m_world->grabTo(m_mirrors.at(object).id, ME, v3d::from(hand / BS));
}

void Body::release(u16 object)
{
	if (!m_grabbing || *m_grabbing != object)
		return;
	m_grabbing.reset();
	auto it = m_mirrors.find(object);
	if (it == m_mirrors.end())
		return;
	m_world->release(it->second.id, ME);
	m_world->hold(it->second.id, true); // the server's again: where it's drawn
}

std::optional<std::pair<v3f, v3f>> Body::grabbed(u16 object) const
{
	if (!m_grabbing || *m_grabbing != object)
		return std::nullopt;
	const physics::Pose p = m_world->pose(m_mirrors.at(object).id);
	return std::make_pair(v3f::from(p.position * BS), workshop::rotationOf(p.rotation));
}

std::optional<std::pair<v3f, v3f>> Body::carried(u16 object) const
{
	if (!m_carrying || m_carrying->first != object)
		return std::nullopt;
	const physics::Pose p = m_world->pose(m_mirrors.at(object).id);
	return std::make_pair(v3f::from(p.position * BS), workshop::rotationOf(p.rotation));
}
