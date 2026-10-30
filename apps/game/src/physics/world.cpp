// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "world.h"

#include <Jolt/Jolt.h>

#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/GroupFilterTable.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/CylinderShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <Jolt/Physics/Constraints/HingeConstraint.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <mutex>

namespace physics {

namespace {

using namespace JPH;

// What touches what. Characters (players) aren't bodies: they move
// themselves, meeting the ground and assemblies, but not what's carried
// (what you carry can't push you, nor you stand on it).
namespace Layer {
constexpr ObjectLayer GROUND = 0, MOVING = 1, CHARACTER = 2, CARRIED = 3, COUNT = 4;
}
namespace Broad {
constexpr BroadPhaseLayer STILL(0), MOVING(1);
constexpr uint COUNT = 2;
}

class Layers final : public BroadPhaseLayerInterface
{
public:
	uint GetNumBroadPhaseLayers() const override { return Broad::COUNT; }
	BroadPhaseLayer GetBroadPhaseLayer(ObjectLayer l) const override
	{
		return l == Layer::GROUND ? Broad::STILL : Broad::MOVING;
	}
#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
	const char *GetBroadPhaseLayerName(BroadPhaseLayer l) const override
	{
		return l == Broad::STILL ? "still" : "moving";
	}
#endif
};

class LayerVsBroad final : public ObjectVsBroadPhaseLayerFilter
{
public:
	bool ShouldCollide(ObjectLayer l, BroadPhaseLayer b) const override
	{
		return l != Layer::GROUND || b == Broad::MOVING;
	}
};

class LayerPairs final : public ObjectLayerPairFilter
{
public:
	bool ShouldCollide(ObjectLayer a, ObjectLayer b) const override
	{
		static constexpr bool TOUCH[Layer::COUNT][Layer::COUNT] = {
			//          ground moving character carried
			/* ground */ { false, true, true, true },
			/* moving */ { true, true, true, true },
			/* char.  */ { true, true, false, false },
			/* carr.  */ { true, true, false, true },
		};
		return TOUCH[a][b];
	}
};

void startJolt()
{
	static std::once_flag once;
	std::call_once(once, [] {
		RegisterDefaultAllocator();
		Factory::sInstance = new Factory();
		RegisterTypes();
	});
}

Vec3 vec(v3f v) { return Vec3(v.X, v.Y, v.Z); }
RVec3 rvec(v3d v) { return RVec3(v.X, v.Y, v.Z); }
v3f out(Vec3 v) { return v3f(v.GetX(), v.GetY(), v.GetZ()); }
v3d rout(RVec3 v) { return v3d(v.GetX(), v.GetY(), v.GetZ()); }
JPH::Quat quat(physics::Quat q) { return JPH::Quat(q.x, q.y, q.z, q.w).Normalized(); }
physics::Quat qout(JPH::Quat q) { return { q.GetX(), q.GetY(), q.GetZ(), q.GetW() }; }

RefConst<Shape> made(ShapeSettings &s)
{
	ShapeSettings::ShapeResult r = s.Create();
	if (r.HasError())
		throw PhysicsError(std::string("physics: ") + r.GetError().c_str());
	return r.Get();
}

// Boxes as one shape: Jolt's compound wants two or more.
RefConst<Shape> compound(std::vector<std::pair<Vec3, Ref<ShapeSettings>>> &parts, const JPH::Quat &turn = JPH::Quat::sIdentity())
{
	if (parts.size() == 1) {
		RotatedTranslatedShapeSettings one(parts[0].first, turn, parts[0].second);
		return made(one);
	}
	StaticCompoundShapeSettings all;
	for (auto &[at, s] : parts)
		all.AddShape(at, turn, s);
	return made(all);
}

Ref<ShapeSettings> box(const aabb3f &b, float density)
{
	const Vec3 half = vec(b.getExtent() * 0.5f);
	const float least = half.ReduceMin();
	if (!(least > 0))
		throw PhysicsError("physics: a box with no size");
	// rounding its corners off by more than it is thick isn't allowed
	auto *s = new BoxShapeSettings(half, std::min(0.01f, least * 0.5f));
	s->SetDensity(density);
	return s;
}

float volume(const aabb3f &b)
{
	const v3f e = b.getExtent();
	return e.X * e.Y * e.Z;
}

// Boxes side by side that would make one box made one: first along x,
// then those rows along z. The ground's 16³ blocks would be thousands of
// boxes (shaped ground is each node in sixteenth slices), and a body
// sliding over a seam between boxes can catch on its edge.
void merge(std::vector<aabb3f> &boxes)
{
	constexpr float E = 1e-4f;
	auto near = [](float a, float b) { return std::fabs(a - b) < E; };
	// along axis `a`, boxes the same across the other two (`b`, `c`) joined end to end
	auto along = [&](int a, int b, int c) {
		std::sort(boxes.begin(), boxes.end(), [&](const aabb3f &p, const aabb3f &q) {
			return std::tie(p.MinEdge[b], p.MinEdge[c], p.MaxEdge[b], p.MaxEdge[c], p.MinEdge[a])
					< std::tie(q.MinEdge[b], q.MinEdge[c], q.MaxEdge[b], q.MaxEdge[c], q.MinEdge[a]);
		});
		std::vector<aabb3f> out;
		for (const aabb3f &x : boxes) {
			if (!out.empty()) {
				aabb3f &last = out.back();
				if (near(last.MinEdge[b], x.MinEdge[b]) && near(last.MaxEdge[b], x.MaxEdge[b])
						&& near(last.MinEdge[c], x.MinEdge[c]) && near(last.MaxEdge[c], x.MaxEdge[c])
						&& near(last.MaxEdge[a], x.MinEdge[a])) {
					last.MaxEdge[a] = x.MaxEdge[a];
					continue;
				}
			}
			out.push_back(x);
		}
		boxes = std::move(out);
	};
	along(0, 1, 2);
	along(2, 1, 0);
}

int blockOf(double node) { return (int)std::floor((node + 0.5) / BLOCK); }

s64 key(v3s16 b)
{
	return ((s64)(b.X & 0xFFFF) << 32) | ((s64)(b.Y & 0xFFFF) << 16) | (s64)(b.Z & 0xFFFF);
}

struct Placed
{
	RVec3 position;
	JPH::Quat rotation;
};

// a then b (b in a's frame)
Placed then(const Placed &a, const Relative &b)
{
	return { a.position + RVec3(a.rotation * vec(b.offset)), a.rotation * quat(b.rotation) };
}

} // namespace

struct World::Impl
{
	Opts opts;
	Layers layers;
	LayerVsBroad layer_vs_broad;
	LayerPairs pairs;
	TempAllocatorImpl temp{ 16 * 1024 * 1024 };
	JobSystemSingleThreaded jobs{ cMaxPhysicsJobs };
	PhysicsSystem system;
	double behind = 0;

	struct Built // an assembly's bodies, compiled
	{
		std::vector<RefConst<Shape>> shapes;
		std::vector<float> friction;
		float mass = 0;
	};
	struct Rec
	{
		Assembly source;
		Built built;
		Tier tier = Tier::Frozen;
		bool held = false;
		bool settled = false;            // it's come to rest since it was made or let go
		Pose pose;                       // frozen: where it is
		std::vector<Relative> rel;       // frozen: where its bodies are, in its root's
		v3f velocity{ 0 };               // frozen: none, unless it was made moving
		std::optional<Pose> target;      // held or carried: where it's to be
		std::optional<float> carried;    // carried: how hard it's pulled there (N)
		Ref<GroupFilterTable> group;
		std::vector<BodyID> bodies;      // live
		std::vector<Ref<Constraint>> hinges;
	};
	std::map<Id, Rec> recs;
	Id next = 1;

	struct Ground
	{
		BodyID body; // invalid: nothing solid in it
		int unused = 0;
	};
	std::map<s64, Ground> ground;

	struct Char
	{
		Ref<CharacterVirtual> c;
		v3f want{ 0 };                // the velocity it moves at next step
		std::optional<RVec3> to;      // placed: where it's to be next step (pushing on the way)
		RVec3 was;                    // its foot before the last step: drawn between
	};
	std::map<CharId, Char> chars;
	CharId next_char = 1;
	std::vector<CharId> players_; // players(): characters placed where each is

	BodyInterface &bi() { return system.GetBodyInterfaceNoLock(); }

	Char &chr(CharId id)
	{
		auto it = chars.find(id);
		if (it == chars.end())
			throw PhysicsError("physics: no character " + std::to_string(id));
		return it->second;
	}

	CharId makeChar(const CharacterSpec &spec, const v3d &foot)
	{
		if (!(spec.radius > 0) || !(spec.height > 2 * spec.radius) || !(spec.mass > 0) || !(spec.strength >= 0))
			throw PhysicsError("physics: a character's height is more than its width, its mass more than 0");
		Ref<CharacterVirtualSettings> s = new CharacterVirtualSettings();
		// its origin at its feet
		RotatedTranslatedShapeSettings shape(Vec3(0, spec.height / 2, 0), JPH::Quat::sIdentity(),
				new CapsuleShape(spec.height / 2 - spec.radius, spec.radius));
		s->mShape = made(shape);
		s->mMaxSlopeAngle = spec.max_slope * core::DEGTORAD;
		s->mMass = spec.mass;
		s->mMaxStrength = spec.strength;
		s->mCharacterPadding = 0.02f;
		s->mPenetrationRecoverySpeed = 1.0f;
		s->mPredictiveContactDistance = 0.1f;
		s->mEnhancedInternalEdgeRemoval = true;
		// only its lower round counts as standing on something
		s->mSupportingVolume = Plane(Vec3::sAxisY(), -spec.radius);
		const CharId id = next_char++;
		Char c;
		c.c = new CharacterVirtual(s, rvec(foot), JPH::Quat::sIdentity(), id, &system);
		c.was = rvec(foot);
		chars.emplace(id, std::move(c));
		return id;
	}

	// The ground round a character, built; false: some isn't loaded.
	bool charGround(const Char &c)
	{
		const RVec3 p = c.to ? *c.to : c.c->GetPosition();
		for (int x = blockOf(p.GetX() - 2); x <= blockOf(p.GetX() + 2); x++)
			for (int y = blockOf(p.GetY() - 2); y <= blockOf(p.GetY() + 3); y++)
				for (int z = blockOf(p.GetZ() - 2); z <= blockOf(p.GetZ() + 2); z++)
					if (!groundAt(v3s16(x, y, z)))
						return false;
		return true;
	}

	void moveChars()
	{
		const Vec3 g = vec(opts.gravity);
		CharacterVirtual::ExtendedUpdateSettings up;
		up.mStickToFloorStepDown = Vec3(0, -0.5f, 0);
		up.mWalkStairsStepUp = Vec3(0, opts.step_up, 0);
		DefaultBroadPhaseLayerFilter broad(layer_vs_broad, Layer::CHARACTER);
		DefaultObjectLayerFilter objects(pairs, Layer::CHARACTER);
		for (auto &[id, c] : chars) {
			c.was = c.c->GetPosition();
			if (!charGround(c)) {
				// in ground that isn't loaded: it waits there
				c.c->SetLinearVelocity(Vec3::sZero());
				continue;
			}
			// placed (a player the client moves): it goes there, pushing
			// what's in the way as hard as it can, and is put there
			const Vec3 v = c.to ? Vec3(*c.to - c.c->GetPosition()) / (float)STEP : vec(c.want);
			c.c->SetLinearVelocity(v);
			c.c->ExtendedUpdate((float)STEP, g, up, broad, objects, BodyFilter(), ShapeFilter(), temp);
			if (c.to) {
				c.c->SetPosition(*c.to);
				c.to.reset();
			}
		}
	}

	Rec &rec(Id id)
	{
		auto it = recs.find(id);
		if (it == recs.end())
			throw PhysicsError("physics: no assembly " + std::to_string(id));
		return it->second;
	}

	static Built build(const Assembly &a)
	{
		if (a.bodies.empty())
			throw PhysicsError("physics: an assembly of no bodies");
		Built out;
		for (const Body &b : a.bodies) {
			if (b.boxes.empty())
				throw PhysicsError("physics: a body of no boxes");
			float mass = 0, rubs = 0;
			std::vector<std::pair<Vec3, Ref<ShapeSettings>>> parts;
			for (const Box &x : b.boxes) {
				mass += volume(x.box) * x.density;
				rubs += volume(x.box) * x.density * x.friction;
				parts.push_back({ vec(x.box.getCenter()), box(x.box, x.density) });
			}
			if (!(mass > 0))
				throw PhysicsError("physics: a body of no mass");
			out.mass += mass;
			out.friction.push_back(rubs / mass);
			if (b.round) {
				const Round &r = *b.round;
				const float v = core::PI * r.radius * r.radius * 2 * r.half_width;
				auto *c = new CylinderShapeSettings(r.half_width, r.radius, std::min(0.02f, r.half_width * 0.5f));
				c->SetDensity(mass / v); // its boxes' mass, as a cylinder
				std::vector<std::pair<Vec3, Ref<ShapeSettings>>> one{ { vec(r.centre), c } };
				out.shapes.push_back(compound(one, JPH::Quat::sFromTo(Vec3::sAxisY(), vec(r.axis).Normalized())));
			} else {
				out.shapes.push_back(compound(parts));
			}
		}
		for (const Hinge &h : a.hinges)
			if (h.a >= a.bodies.size() || h.b >= a.bodies.size() || h.a == h.b)
				throw PhysicsError("physics: a hinge between bodies it hasn't");
		return out;
	}

	// Into Jolt where it's frozen.
	void wake(Id id, Rec &r)
	{
		const size_t n = r.built.shapes.size();
		r.group = new GroupFilterTable((uint)n);
		for (uint i = 0; i < n; i++)
			for (uint j = i + 1; j < n; j++)
				r.group->DisableCollision(i, j);
		const Placed root{ rvec(r.pose.position), quat(r.pose.rotation) };
		r.rel.resize(n);
		for (size_t i = 0; i < n; i++) {
			const Placed at = i == 0 ? root : then(root, r.rel[i]);
			BodyCreationSettings s(r.built.shapes[i], at.position, at.rotation,
					r.held ? EMotionType::Kinematic : EMotionType::Dynamic, r.carried ? Layer::CARRIED : Layer::MOVING);
			s.mAllowDynamicOrKinematic = true;
			s.mFriction = r.built.friction[i];
			s.mRestitution = 0.05f;
			s.mLinearDamping = 0.05f;
			s.mAngularDamping = 0.1f;
			s.mCollisionGroup = CollisionGroup(r.group, id, (CollisionGroup::SubGroupID)i);
			s.mLinearVelocity = vec(r.velocity);
			// over the ground's seams without catching on their edges
			s.mEnhancedInternalEdgeRemoval = true;
			const BodyID b = bi().CreateAndAddBody(s, EActivation::Activate);
			if (b.IsInvalid())
				throw PhysicsError("physics: too many bodies");
			r.bodies.push_back(b);
		}
		for (const Hinge &h : r.source.hinges) {
			HingeConstraintSettings s;
			s.mSpace = EConstraintSpace::WorldSpace;
			const Vec3 axis = (root.rotation * vec(h.axis)).Normalized();
			s.mPoint1 = s.mPoint2 = root.position + RVec3(root.rotation * vec(h.anchor));
			s.mHingeAxis1 = s.mHingeAxis2 = axis;
			s.mNormalAxis1 = s.mNormalAxis2 = axis.GetNormalizedPerpendicular();
			s.mMaxFrictionTorque = h.friction;
			Ref<Constraint> c = bi().CreateConstraint(&s, r.bodies[h.a], r.bodies[h.b]);
			system.AddConstraint(c);
			r.hinges.push_back(c);
		}
		r.tier = Tier::Live;
	}

	// Out of Jolt, kept as where it is.
	void freeze(Rec &r)
	{
		if (r.tier == Tier::Frozen)
			return;
		r.pose = livePose(r);
		r.rel = liveRelatives(r);
		r.velocity = v3f(0);
		for (Ref<Constraint> &c : r.hinges)
			system.RemoveConstraint(c);
		r.hinges.clear();
		for (const BodyID &b : r.bodies) {
			bi().RemoveBody(b);
			bi().DestroyBody(b);
		}
		r.bodies.clear();
		r.tier = Tier::Frozen;
	}

	Placed placed(const BodyID &b)
	{
		RVec3 p;
		JPH::Quat q;
		bi().GetPositionAndRotation(b, p, q);
		return { p, q };
	}

	Pose livePose(Rec &r)
	{
		const Placed p = placed(r.bodies[0]);
		return { rout(p.position), qout(p.rotation) };
	}

	std::vector<Relative> liveRelatives(Rec &r)
	{
		std::vector<Relative> all(r.bodies.size());
		const Placed root = placed(r.bodies[0]);
		const JPH::Quat back = root.rotation.Conjugated();
		for (size_t i = 1; i < r.bodies.size(); i++) {
			const Placed b = placed(r.bodies[i]);
			all[i] = { out(back * Vec3(b.position - root.position)), qout(back * b.rotation) };
		}
		return all;
	}

	// The ground blocks a live assembly needs, or nothing if one isn't loaded.
	bool needs(Rec &r, std::vector<v3s16> &blocks)
	{
		for (const BodyID &b : r.bodies) {
			BodyLockRead lock(system.GetBodyLockInterfaceNoLock(), b);
			const AABox box = lock.GetBody().GetWorldSpaceBounds();
			const v3s16 lo(blockOf(box.mMin.GetX() - 2), blockOf(box.mMin.GetY() - 2), blockOf(box.mMin.GetZ() - 2));
			const v3s16 hi(blockOf(box.mMax.GetX() + 2), blockOf(box.mMax.GetY() + 2), blockOf(box.mMax.GetZ() + 2));
			for (s16 x = lo.X; x <= hi.X; x++)
				for (s16 y = lo.Y; y <= hi.Y; y++)
					for (s16 z = lo.Z; z <= hi.Z; z++)
						if (!groundAt(v3s16(x, y, z)))
							return false;
		}
		return true;
	}

	// The block built (if it isn't), kept a while longer; false: not loaded.
	bool groundAt(v3s16 block)
	{
		auto it = ground.find(key(block));
		if (it != ground.end()) {
			it->second.unused = 0;
			return true;
		}
		std::vector<aabb3f> boxes;
		if (!opts.ground(block, boxes))
			return false;
		ground[key(block)] = { makeGround(block, boxes), 0 };
		return true;
	}

	BodyID makeGround(v3s16 block, std::vector<aabb3f> &boxes)
	{
		if (boxes.empty())
			return BodyID();
		merge(boxes);
		std::vector<std::pair<Vec3, Ref<ShapeSettings>>> parts;
		for (const aabb3f &b : boxes)
			parts.push_back({ vec(b.getCenter()), box(b, 1000) });
		BodyCreationSettings s(compound(parts), RVec3(block.X * BLOCK, block.Y * BLOCK, block.Z * BLOCK),
				JPH::Quat::sIdentity(), EMotionType::Static, Layer::GROUND);
		s.mFriction = 0.7f;
		const BodyID b = bi().CreateAndAddBody(s, EActivation::DontActivate);
		if (b.IsInvalid())
			throw PhysicsError("physics: too many bodies");
		return b;
	}

	void dropGround(Ground &g)
	{
		if (g.body.IsInvalid())
			return;
		bi().RemoveBody(g.body);
		bi().DestroyBody(g.body);
	}

	double nearest(const v3d &p) const
	{
		double d = std::numeric_limits<double>::infinity();
		// every character's someone: players placed, and those that walk
		for (const auto &[id, c] : chars)
			d = std::min(d, rout(c.to ? *c.to : c.c->GetPosition()).getDistanceFrom(p));
		return d;
	}

	bool asleep(const Rec &r)
	{
		for (const BodyID &b : r.bodies)
			if (bi().IsActive(b))
				return false;
		return true;
	}

	// Which assemblies are Live, and the ground round those that are.
	void tiers()
	{
		for (auto &[id, r] : recs) {
			const v3d at = r.tier == Tier::Live ? livePose(r).position : r.pose.position;
			const double d = nearest(at);
			if (r.tier == Tier::Frozen && (r.held || r.carried || !r.settled || d < opts.live)) {
				wake(id, r);
				std::vector<v3s16> unused;
				if (!needs(r, unused))
					freeze(r); // its ground isn't loaded: it waits, as it is
			} else if (r.tier == Tier::Live) {
				std::vector<v3s16> unused;
				if (!r.held && !r.carried && asleep(r))
					r.settled = true;
				if (!r.held && !r.carried && (!needs(r, unused) || (d > opts.freeze && r.settled && asleep(r))))
					freeze(r);
			}
		}
		for (auto it = ground.begin(); it != ground.end();) {
			if (++it->second.unused > 120) { // two seconds needed by nothing
				dropGround(it->second);
				it = ground.erase(it);
			} else {
				++it;
			}
		}
	}

	// Carried: pulled toward where it's to be (and turned to its turn) as
	// hard as the carrier can, against its weight: what's light follows the
	// hands, what's heavy lags, what's too heavy to lift drags.
	void pull(Rec &r)
	{
		constexpr float TAU = 0.06f, FASTEST = 20, SPIN = 20; // s to close the gap; nodes/s; rad/s
		const BodyID root = r.bodies[0];
		const Placed at = placed(root);
		float mass;
		{
			BodyLockRead lock(system.GetBodyLockInterfaceNoLock(), root);
			mass = 1 / std::max(1e-6f, lock.GetBody().GetMotionProperties()->GetInverseMass());
		}
		const float most = *r.carried / mass; // nodes/s² the carrier can give it
		const Vec3 g = vec(opts.gravity), v = bi().GetLinearVelocity(root);
		Vec3 want = Vec3(rvec(r.target->position) - at.position) / TAU;
		if (want.Length() > FASTEST)
			want = want.Normalized() * FASTEST;
		Vec3 a = (want - v) / (float)STEP - g; // what it needs besides its weight
		if (a.Length() > most)
			a = a.Normalized() * most;
		bi().SetLinearVelocity(root, v + a * (float)STEP); // the step adds its weight
		// turned the same way, as hard (its reach as half a node)
		JPH::Quat d = quat(r.target->rotation) * at.rotation.Conjugated();
		if (d.GetW() < 0)
			d = -d;
		Vec3 axis;
		float angle;
		d.GetAxisAngle(axis, angle);
		Vec3 spin = angle > 1e-5f ? axis * std::min(SPIN, angle / TAU) : Vec3::sZero();
		const Vec3 w = bi().GetAngularVelocity(root);
		Vec3 dw = spin - w;
		const float turns = most * 2 * (float)STEP;
		if (dw.Length() > turns)
			dw = dw.Normalized() * turns;
		bi().SetAngularVelocity(root, w + dw);
	}

	void steer()
	{
		for (auto &[id, r] : recs) {
			if (r.tier == Tier::Live && r.carried && r.target)
				pull(r);
			if (r.tier != Tier::Live || !r.held || !r.target)
				continue;
			const Placed root{ rvec(r.target->position), quat(r.target->rotation) };
			const std::vector<Relative> rel = liveRelatives(r);
			for (size_t i = 0; i < r.bodies.size(); i++) {
				const Placed at = i == 0 ? root : then(root, rel[i]);
				bi().MoveKinematic(r.bodies[i], at.position, at.rotation, (float)STEP);
			}
		}
	}
};

// Jolt's allocator is registered before Impl's members use it.
World::World(const Opts &opts) : m((startJolt(), std::make_unique<Impl>()))
{
	if (!opts.ground)
		throw PhysicsError("physics: a world needs its ground");
	m->opts = opts;
	m->system.Init(65536, 0, 65536, 16384, m->layers, m->layer_vs_broad, m->pairs);
	m->system.SetGravity(vec(opts.gravity));
}

World::~World()
{
	for (auto &[id, r] : m->recs)
		m->freeze(r);
	for (auto &[k, g] : m->ground)
		m->dropGround(g);
	m->chars.clear();
}

World::Id World::add(const Assembly &a, const Pose &pose, v3f velocity)
{
	Impl::Rec r;
	r.built = Impl::build(a);
	r.source = a;
	r.pose = pose;
	r.velocity = velocity;
	r.rel.resize(a.bodies.size());
	const Id id = m->next++;
	m->recs.emplace(id, std::move(r));
	return id;
}

void World::remove(Id id)
{
	m->freeze(m->rec(id));
	m->recs.erase(id);
}

bool World::has(Id id) const { return m->recs.count(id) != 0; }

void World::rebuild(Id id, const Assembly &a)
{
	Impl::Rec &r = m->rec(id);
	Impl::Built built = Impl::build(a);
	const bool live = r.tier == Tier::Live;
	const v3f v = live ? out(m->bi().GetLinearVelocity(r.bodies[0])) : r.velocity;
	m->freeze(r);
	r.built = std::move(built);
	r.source = a;
	r.velocity = v;
	r.settled = false;
	// bodies came and went: they start where its root is
	if (r.rel.size() != a.bodies.size())
		r.rel.assign(a.bodies.size(), Relative{});
	if (live)
		m->wake(id, r);
}

void World::hold(Id id, bool held)
{
	Impl::Rec &r = m->rec(id);
	if (held && r.carried)
		carry(id, std::nullopt);
	r.held = held;
	r.settled = false;
	r.target.reset();
	if (r.tier != Tier::Live)
		return;
	for (const BodyID &b : r.bodies)
		m->bi().SetMotionType(b, held ? EMotionType::Kinematic : EMotionType::Dynamic, EActivation::Activate);
}

void World::moveTo(Id id, const Pose &to)
{
	Impl::Rec &r = m->rec(id);
	if (!r.held)
		throw PhysicsError("physics: only what's held is moved to a pose");
	r.target = to;
	if (r.tier == Tier::Frozen)
		r.pose = to;
}

void World::impulse(Id id, v3f impulse)
{
	Impl::Rec &r = m->rec(id);
	r.settled = false;
	if (r.tier == Tier::Frozen)
		m->wake(id, r);
	m->bi().AddImpulse(r.bodies[0], vec(impulse));
}

void World::players(const std::vector<Capsule> &players)
{
	while (m->players_.size() > players.size()) {
		m->chars.erase(m->players_.back());
		m->players_.pop_back();
	}
	for (size_t i = 0; i < players.size(); i++) {
		const Capsule &p = players[i];
		if (i == m->players_.size())
			m->players_.push_back(m->makeChar({ p.radius, p.height, p.mass, p.strength, 50 }, p.foot));
		m->chr(m->players_[i]).to = rvec(p.foot);
	}
}

World::CharId World::addCharacter(const CharacterSpec &spec, const v3d &foot) { return m->makeChar(spec, foot); }

void World::removeCharacter(CharId id)
{
	m->chr(id);
	m->chars.erase(id);
}

void World::want(CharId id, v3f velocity) { m->chr(id).want = velocity; }

void World::place(CharId id, const v3d &foot, v3f velocity)
{
	Impl::Char &c = m->chr(id);
	c.c->SetPosition(rvec(foot));
	c.c->SetLinearVelocity(vec(velocity));
	c.was = rvec(foot);
	c.want = velocity;
}

World::CharacterState World::character(CharId id) const
{
	Impl::Char &c = m->chr(id);
	const bool on = c.c->GetGroundState() == CharacterVirtual::EGroundState::OnGround;
	return { rout(c.c->GetPosition()), out(c.c->GetLinearVelocity()), on,
		on ? out(c.c->GetGroundVelocity()) : v3f(0), on ? out(c.c->GetGroundNormal()) : v3f(0, 1, 0) };
}

v3d World::drawn(CharId id) const
{
	Impl::Char &c = m->chr(id);
	const double t = std::clamp(m->behind / STEP, 0.0, 1.0);
	return rout(c.was) + (rout(c.c->GetPosition()) - rout(c.was)) * t;
}

void World::carry(Id id, std::optional<float> strength)
{
	Impl::Rec &r = m->rec(id);
	if (strength && !(*strength > 0))
		throw PhysicsError("physics: a carrier's strength is more than 0");
	if (r.held)
		hold(id, false);
	r.carried = strength;
	r.settled = false;
	if (!strength)
		r.target.reset();
	if (r.tier != Tier::Live)
		return;
	for (const BodyID &b : r.bodies) {
		m->bi().SetObjectLayer(b, strength ? Layer::CARRIED : Layer::MOVING);
		m->bi().ActivateBody(b);
	}
}

void World::pullTo(Id id, const Pose &to)
{
	Impl::Rec &r = m->rec(id);
	if (!r.carried)
		throw PhysicsError("physics: only what's carried is pulled to a pose");
	r.target = to;
}

bool World::carried(Id id) const { return m->rec(id).carried.has_value(); }
bool World::held(Id id) const { return m->rec(id).held; }

void World::groundChanged(v3s16 block)
{
	auto it = m->ground.find(key(block));
	if (it == m->ground.end())
		return;
	m->dropGround(it->second);
	m->ground.erase(it);
	// what may have stood on it looks again
	for (auto &[id, r] : m->recs)
		for (const BodyID &b : r.bodies)
			m->bi().ActivateBody(b);
}

void World::step(double dt)
{
	m->behind += dt;
	int steps = 0;
	// a server steps every 0.09 s by default (5–6 of ours): up to a fifth of
	// a second is caught up, more is dropped
	constexpr int MOST = 12;
	while (m->behind >= STEP && steps < MOST) {
		m->tiers();
		m->steer();
		m->moveChars();
		m->system.Update((float)STEP, 1, &m->temp, &m->jobs);
		m->behind -= STEP;
		steps++;
	}
	if (steps == MOST)
		m->behind = 0; // too far behind: the time's dropped, not caught up
}

Pose World::pose(Id id) const
{
	Impl::Rec &r = m->rec(id);
	return r.tier == Tier::Live ? m->livePose(r) : r.pose;
}

v3f World::velocity(Id id) const
{
	Impl::Rec &r = m->rec(id);
	return r.tier == Tier::Live ? out(m->bi().GetLinearVelocity(r.bodies[0])) : r.velocity;
}

float World::mass(Id id) const { return m->rec(id).built.mass; }

Tier World::tier(Id id) const { return m->rec(id).tier; }

bool World::asleep(Id id) const
{
	Impl::Rec &r = m->rec(id);
	return r.tier == Tier::Frozen || m->asleep(r);
}

std::vector<Relative> World::relatives(Id id) const
{
	Impl::Rec &r = m->rec(id);
	std::vector<Relative> all = r.tier == Tier::Live ? m->liveRelatives(r) : r.rel;
	all.erase(all.begin());
	return all;
}

size_t World::groundBlocks() const { return m->ground.size(); }

} // namespace physics
