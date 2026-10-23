// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "content/subgames.h"
#include "matter.h"
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

	// Runs frames until the player picks a world, or quits (nullopt).
	// `error` is why the last session ended, if it ended badly.
	std::optional<WorldSpec> run(const std::string &error);

private:
	Shell &m_shell;
	RenderingEngine *m_engine;
	const Worlds &m_worlds;
	const Lab &m_lab;
	MatterVerbs m_matter;
	std::optional<WorldSpec> m_choice;
	bool m_quit = false;
};

} // namespace shell
