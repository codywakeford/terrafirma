// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "entities.h"
#include "library.h"
#include "motion.h"
#include "piece/machine.h"
#include "sculpt/sculpt.h"
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

class IGameDef;
class LuaEntitySAO;
class PlayerSAO;
class ServerEnvironment;

namespace workshop {

// What a player asked that can't be done, in words for them.
struct Refused : std::runtime_error
{
	using std::runtime_error::runtime_error;
};

/*
	Workshop: what players make (sprints 48–52), the server's side of the
	tools in hand. Strokes on the world and on machines' parts, undoing
	them; machines made, held (kept still, posed by their holder) and let
	go to fall; a stroke drawn as a part of its own (a wheel: a cylinder,
	hinged), joints set between parts, two machines joined face to face,
	a part taken off or deleted; the library: a
	machine kept as a blueprint, placed again. In creative what's built
	follows the design. Each verb is for a player the caller has let use
	the tools (interact, creative), and throws Refused on what can't be.

	A machine is named here by its entity's object id (what the client
	points at), a part by its own id; the history keeps the machine's id,
	which lasts.
*/
class Workshop
{
public:
	struct Opts
	{
		ServerEnvironment &env;
		IGameDef *gamedef;
		std::string world_path;
		const Materials &materials;
	};
	explicit Workshop(const Opts &opts);

	// On the world (object 0) or a machine's part: the block held is
	// placed (or painted), else the material pointed at. `apart`: what it
	// adds grown as a part of its own off the part (and its mirror's
	// another), joined to it so (a hinge: about the face's normal).
	void stroke(PlayerSAO &who, u16 object, u64 part, const sculpt::Stroke &s,
			std::optional<piece::Joint> apart);
	void undo(PlayerSAO &who);
	// A new machine, one block of the block held, held up in front of the
	// player: its object id.
	u16 start(PlayerSAO &who);
	// Held, it's kept still where its holder poses it; let go, it falls.
	void hold(PlayerSAO &who, u16 object, bool held);
	// Where a held machine is (BS) and how it's turned (degrees): within reach.
	void pose(PlayerSAO &who, u16 object, v3f position, v3f rotation);
	// A part's design made a shell `thickness` sixteenths thick (undone as a stroke is).
	void hollow(PlayerSAO &who, u16 object, u64 part, int thickness);

	// The joint between two of a machine's parts set to `joint` (made at
	// a face of a's, if there's none); none: taken away.
	void relink(PlayerSAO &who, u16 object, u64 a, u64 b, std::optional<piece::Joint> joint,
			const std::optional<piece::Machine::Face> &at);
	// Machine `b` set against `a` face to face and made one with it.
	void join(PlayerSAO &who, u16 a, const piece::Machine::Face &on_a, u16 b, const piece::Machine::Face &on_b,
			piece::Joint joint);
	// A socket made on a face (and its mirror's, across the machine's
	// plane), or taken away.
	void socket(PlayerSAO &who, u16 object, const piece::Machine::Face &face, piece::Joint joint,
			const std::optional<sculpt::Stroke::Mirror> &mirror);
	void unsocket(PlayerSAO &who, u16 object, u64 socket);
	// Machine `b` carried onto `a`'s socket, joined there.
	void plug(PlayerSAO &who, u16 a, u64 socket, u16 b, const piece::Machine::Face &on_b);
	// A part made a machine of its own, where it was.
	void detach(PlayerSAO &who, u16 object, u64 part);
	// A part gone (the machine with it, if it's its last).
	void remove(PlayerSAO &who, u16 object, u64 part);

	void save(PlayerSAO &who, u16 object, const std::string &name);
	// Its machines, from the open node `at`.
	void place(PlayerSAO &who, const std::string &name, v3s16 at);
	std::vector<std::string> blueprints(const std::string &player) const { return m_library.names(player); }

	// Machines moved by physics (sprint 57): each server step, before the
	// environment's; and told when the world's blocks change.
	void step(float dtime) { m_motion.step(dtime); }
	void mapEdited(const MapEditEvent &e) { m_motion.mapEdited(e); }
	Motion &motion() { return m_motion; }

private:
	void within(PlayerSAO &who, v3f there, float extra) const;
	content_t held(PlayerSAO &who) const;

	ServerEnvironment &m_env;
	IGameDef *m_gamedef;
	Entities m_entities;
	Library m_library;
	sculpt::History m_history;
	Motion m_motion;
};

} // namespace workshop
