// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "content/subgames.h"
#include "online/join.h"
#include <mutex>
#include <optional>
#include <string>

class RenderingEngine;

namespace shell {

class Shell;
class Worlds;
class Lab;

/*
	The main menu: the page draws it, this answers it. While a Menu exists the
	page can list, create and delete worlds, and choose one to play.
*/
// What the player chose: a world of their own (the Lab's are never shared), or a friend's.
struct Choice
{
	std::optional<WorldSpec> world;
	bool lab = false;
	std::optional<online::JoinPlan> join;
};

class Menu
{
public:
	struct Opts {
		Shell &shell;
		RenderingEngine *engine;
		const Worlds &worlds;
		const Lab &lab;
	};

	explicit Menu(const Opts &opts);
	~Menu();

	// Runs frames until the player picks a world or a friend's, or quits
	// (nullopt). `error` is why the last session ended, if it ended badly.
	std::optional<Choice> run(const std::string &error);

private:
	Shell &m_shell;
	RenderingEngine *m_engine;
	const Worlds &m_worlds;
	const Lab &m_lab;
	std::optional<Choice> m_choice;
	bool m_quit = false;
	// Set on the bridge's worker once a join is ready; taken by run().
	std::mutex m_join_lock;
	std::optional<online::JoinPlan> m_join;
};

} // namespace shell
