// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "piece/object.h"
#include "shape/shape.h"
#include "irr_aabb3d.h"
#include <optional>
#include <string>
#include <vector>

class IGameDef;

/*
	A machine (sprint 52): parts, each an object (a design and what's built
	of it) set in the machine's grid, and the joints between them. A cart's
	chassis, its axles, its wheels. One entity carries it, and it moves and
	falls as one; a hinge's part is free to turn about its axle (when
	there's something to turn it: sprint 53).

	A part sits at an offset turned one of the 24 square turns: every
	part's sixteenths are the machine's, so the tools, and parts joined
	face to face, are exact. Points are in sixteenths from a grid's corner
	(node (0, 0, 0)'s least): a part's sixteenth (n, c) spans 16n + c to
	16n + c + 1; the first part's are the machine's.

	Its text is "TFM3", its id, its parts, its joints, its sockets (TFM2
	had no sockets; TFM1, sprint 52, had what each part was made as: read
	past). An object's text
	(TFO1) or a piece's from before reads as a machine of one part.
*/
namespace piece {

struct Pose
{
	v3s32 offset; // where the part's corner is, in the machine's sixteenths
	u8 turn = 0;  // piece::turn

	// A point of the part's (sixteenths) in the machine's.
	v3f apply(v3f local) const;
	// A point of the machine's in the part's.
	v3f unapply(v3f machine) const;
	// The part's frame (BS) in the machine's.
	core::matrix4 matrix() const;
};

// What a joint is, by kind.
enum class Joint : u8 {
	Fixed, // welded
	Hinge, // turning about its axis
	Count,
};
struct JointKind
{
	const char *id;
	bool turns; // about its axis
};
extern const JointKind JOINTS[size_t(Joint::Count)];

class Machine
{
public:
	struct Part
	{
		Object object;
		Pose pose;
	};
	struct Link
	{
		u64 a, b; // the parts' ids
		Joint joint;
		v3f anchor; // sixteenths, the machine's
		v3s16 axis; // a unit axis, the machine's: a hinge's
	};
	// A joint waiting for a part (sprint 56): on a part's face, its axis
	// out of it. Something carried onto it is joined there, so, and it's
	// taken up.
	struct Socket
	{
		u64 id;
		u64 part;
		v3f point;  // sixteenths, the machine's
		v3s16 axis; // a unit axis, the machine's: out of the face
		Joint joint;
	};

	// A new machine: no parts yet, an id of its own.
	explicit Machine(IGameDef *gamedef);

	// Throws SerializationError on text that isn't a machine, an object
	// or a piece.
	void read(const std::string &text);
	std::string write() const;

	u64 id() const { return m_id; }
	// A copy (a blueprint placed): ids of its own, its parts' too.
	void renew();

	std::vector<Part> &parts() { return m_parts; }
	const std::vector<Part> &parts() const { return m_parts; }
	std::vector<Link> &links() { return m_links; }
	const std::vector<Link> &links() const { return m_links; }
	const std::vector<Socket> &sockets() const { return m_sockets; }
	Part *part(u64 id);
	const Part *part(u64 id) const;
	Part &add(Part part);
	// A part taken out, with its joints; the rest stay as they are.
	Part take(u64 id);

	// A face of a part: the sixteenth pointed at and its face's normal
	// (the part's own).
	struct Face
	{
		u64 part;
		shape::Sixteenth at;
		v3s16 normal;
	};
	// A socket made on a face (throws on a face of no part); one taken away.
	const Socket &socket(const Face &face, Joint joint);
	void unsocket(u64 id);
	// `other` carried onto socket `id`: its face `theirs` set flat against
	// the socket's face, middles met, joined there as the socket says (a
	// hinge about its axis), the socket taken up. Throws on no such socket.
	void plug(u64 id, Machine other, const Face &theirs);
	// Where on it (the machine's sixteenths: the face's middle) and which
	// way it faces.
	v3f point(const Face &face) const;
	v3s16 normal(const Face &face) const;
	// Room made on a part's grid for nodes `lo` to `hi` (its own, as they'd
	// be): what's on it moved along it, and the part the other way in the
	// machine, so nothing moves in the world. How far it moved (none: there
	// was room). Throws if they and what's there won't fit on a grid.
	v3s16 makeRoom(u64 part, v3s32 lo, v3s32 hi);
	// A part of its own grown out of a face: `cells` (the face's part's
	// sixteenths: a stroke's) of `material`, on its grid where they are,
	// joined to the face's part by `joint` at the face's point (a hinge
	// about its normal). Throws on cells further apart than a part holds.
	Part &grow(const Face &face, const std::vector<v3s32> &cells, content_t material, Joint joint);
	// The joint between two parts set to `joint`; none there, made at
	// `at` (a face of a's), or at a's corner if there's none given.
	void relink(u64 a, u64 b, Joint joint, const std::optional<Face> &at);
	// The joint between them taken away (none there: nothing's done).
	void unlink(u64 a, u64 b);
	// Its face furthest along `d` (a unit axis, the machine's): of the
	// node that reaches furthest that way (of those, the nearest its
	// middle across), the middle of that node's face. What's carried onto
	// another is joined by it. Nothing if it has no parts.
	std::optional<Face> faceToward(v3s16 d) const;
	// The face across the machine's plane of axis `axis` at half `twice`
	// (its sixteenths), on the same part (a pair of wheels' second).
	Face mirrored(const Face &face, int axis, s32 twice) const;
	// `other`'s parts and joints made this one's: its face set flat
	// against this one's (turned square, as little as it can be), joined
	// there by `joint` about this face's normal.
	void join(const Face &mine, Machine other, const Face &theirs, Joint joint);

	// Its parts in rigid groups: welded together, they move as one; a
	// hinge is between two groups. The first part's group is first; parts
	// no joint reaches are in it. Each group's parts in the machine's order.
	std::vector<std::vector<u64>> rigid() const;

	// Every part built as designed (creative).
	void build();
	bool empty() const;
	// Its designed and built extents, in the machine's nodes (BS = 1).
	std::optional<aabb3f> designed() const;
	std::optional<aabb3f> built() const;

private:
	IGameDef *m_gamedef;
	u64 m_id;
	std::vector<Part> m_parts;
	std::vector<Link> m_links;
	std::vector<Socket> m_sockets;

	// `other`'s parts, joints and sockets made this one's, its face
	// `theirs` set flat at `p` facing against `n`.
	void adopt(Machine other, const Face &theirs, v3f p, v3s16 n);
};

} // namespace piece
