// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "session.h"
#include "bridge.h"
#include "shell.h"
#include "preview.h"
#include "gui/mainmenumanager.h"
#include "porting.h"

namespace shell {

static const char *const VERBS[] = {
	"screen.close", "game.leave", "game.quit",
	"inventory.move", "inventory.craft", "inventory.drop", "creative.list", "ui.send", "screen.open", "worldgen.preview",
};

Session::Session(const Opts &opts) :
	m_shell(opts.shell),
	m_page(opts.shell.pageLoads()),
	m_items({ opts.client, opts.driver, opts.shell.itemDir(), std::to_string(porting::getTimeMs()) }),
	m_state({ opts.shell, opts.client, m_items }),
	m_ui({ opts.shell, opts.client }),
	m_director({ m_ui, opts.client }),
	m_screens({ opts.shell, opts.guienv, m_ui }),
	m_actions(opts.client)
{
	m_shell.on("inventory.move", [this](const Json::Value &intent) {
		m_actions.move(intent);
		return Json::Value();
	});
	m_shell.on("inventory.craft", [this](const Json::Value &intent) {
		m_actions.craft(intent);
		return Json::Value();
	});
	m_shell.on("inventory.drop", [this](const Json::Value &intent) {
		m_actions.drop(intent);
		return Json::Value();
	});
	m_shell.on("creative.list", [this](const Json::Value &) {
		return m_items.catalog();
	});
	m_shell.onAsync("worldgen.preview", [](const Json::Value &intent) {
		return previewJson(intent);
	});
	m_shell.on("ui.send", [this](const Json::Value &intent) {
		m_ui.send(intent);
		return Json::Value();
	});
	// The page opens a screen itself where it decides when (the Lab browser
	// on arrival). Only screens in the table, and only where offered.
	m_shell.on("screen.open", [this](const Json::Value &intent) {
		if (!intent["name"].isString())
			throw Refused("screen.open needs a string 'name'");
		m_screens.open(intent["name"].asString());
		return Json::Value();
	});
	m_shell.on("screen.close", [this](const Json::Value &) {
		m_screens.close();
		return Json::Value();
	});
	m_shell.on("game.leave", [](const Json::Value &) {
		g_gamecallback->disconnect(); // back to the main menu
		return Json::Value();
	});
	m_shell.on("game.quit", [](const Json::Value &) {
		g_gamecallback->exitToOS();
		return Json::Value();
	});
}

void Session::update(const HudView &view)
{
	if (m_shell.pageLoads() != m_page) { // a reloaded page: tell it everything again
		m_page = m_shell.pageLoads();
		m_state.forget();
		m_ui.replay();
	}
	m_state.update(view);
	m_ui.update();
	m_director.update();
}

Session::~Session()
{
	for (const char *verb : VERBS)
		m_shell.off(verb);
}

} // namespace shell
