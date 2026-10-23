// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "actions.h"
#include "items.h"
#include "screens.h"
#include "state.h"
#include "director.h"
#include "ui.h"

class Client;
namespace video { class IVideoDriver; }
namespace gui { class IGUIEnvironment; }

namespace shell {

class Shell;

/*
	One running game, as the page sees it. Exists from the moment the client
	has its content (item and node definitions) until the game shuts down,
	and answers the in-game intents for exactly that long.
*/
class Session
{
public:
	struct Opts {
		Shell &shell;
		Client *client;
		video::IVideoDriver *driver;
		gui::IGUIEnvironment *guienv;
	};

	explicit Session(const Opts &opts);
	~Session();

	// Once per frame, before drawing.
	void update(const HudView &view)
	{
		m_state.update(view);
		m_ui.update();
		m_director.update();
	}

	Screens &screens() { return m_screens; }

private:
	Shell &m_shell;
	Items m_items;
	State m_state;
	Ui m_ui;
	Director m_director;
	Screens m_screens;
	Actions m_actions;
};

} // namespace shell
