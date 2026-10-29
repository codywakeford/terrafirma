// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "session.h"
#include "bridge.h"
#include "client/client.h"
#include "terragen/worlds.h"
#include "client/localplayer.h"
#include "shell.h"
#include "preview.h"
#include "system.h"
#include "gui/mainmenumanager.h"
#include "porting.h"

namespace shell {

static const char *const VERBS[] = {
	"screen.close", "game.leave", "game.quit",
	"inventory.move", "inventory.craft", "inventory.drop", "creative.list", "ui.send", "screen.open", "worldgen.preview",
	"system.preview", "system.world",
};

Session::Session(const Opts &opts) :
	m_shell(opts.shell),
	m_page(opts.shell.pageLoads()),
	m_items({ opts.client, opts.driver, opts.shell.itemDir(), std::to_string(porting::getTimeMs()) }),
	m_state({ opts.shell, opts.client, m_items }),
	m_ui({ opts.shell, opts.client }),
	m_director({ m_ui, opts.client }),
	m_screens({ opts.shell, opts.guienv, m_ui }),
	m_actions(opts.client),
	m_client(opts.client),
	m_planetarium(opts.shell.planetarium())
{
	// the page shows this world's bodies: `body { code, body? }`, the
	// home's when it names none
	m_planetarium.serve([this](const Json::Value &body) { return world(bodyNamed(body)); });
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
	// a body of this world names it (its world isn't a code of its own);
	// the menu's previews are codes alone
	m_shell.onAsync("worldgen.preview", [this](const Json::Value &intent) {
		if (!intent.isMember("body"))
			return previewJson(intent);
		return previewJson(intent, bodies().at(bodyNamed(intent)).code);
	});
	// the map's bodies as the builder shows them: each one's card (stateless, as the menu's)
	m_shell.on("system.preview", [](const Json::Value &intent) {
		return systemPreview(intent);
	});
	// the world's own system, for the map's bodies
	m_shell.on("system.world", [](const Json::Value &intent) {
		return worldSystem(intent);
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
	// the system as the world's date has it (its year's share of the orbit)
	if (m_planetarium.shown() && m_year > 0)
		m_planetarium.dayIn(m_client->getEnv().getDayCount() + m_client->getEnv().getTimeOfDayF(), m_year);
}

std::vector<terragen::BodyWorld> Session::bodies() const
{
	const planet::Atlas *atlas = m_client->getAtlas();
	const std::optional<far::Horizon> &horizon = m_client->getHorizon();
	if (!atlas || !horizon)
		throw Refused("this world isn't a planet: it has no bodies");
	return terragen::bodyWorlds(terragen::WorldCode::decode(horizon->code), atlas->body(planet::Atlas::HOME).params());
}

int Session::bodyNamed(const Json::Value &intent) const
{
	if (!intent.isMember("body"))
		return planet::Atlas::HOME;
	if (!intent["body"].isString())
		throw Refused("a body is named by its id (a string)");
	const std::string id = intent["body"].asString();
	const std::vector<terragen::BodyWorld> worlds = bodies();
	for (size_t b = 0; b < worlds.size(); b++)
		if (worlds[b].id == id)
			return (int)b;
	throw Refused("this world has no body '" + id + "' to show");
}

int Session::bodyHere() const
{
	const planet::Atlas *atlas = m_client->getAtlas();
	LocalPlayer *player = m_client->getEnv().getLocalPlayer();
	if (!atlas || !player)
		return planet::Atlas::HOME;
	const v3f feet = player->getPosition() / BS;
	return atlas->bodyAt(planet::Atlas::moverNode(feet.X, feet.Y, feet.Z)).value_or(planet::Atlas::HOME);
}

planetarium::Planetarium::Opts Session::world(int body) const
{
	const planet::Atlas *atlas = m_client->getAtlas();
	const std::optional<far::Horizon> &horizon = m_client->getHorizon();
	if (!atlas || !horizon)
		throw Refused("the Planetarium shows a planet, and this world isn't one");
	far::Palette palette = far::Palette::live(m_client->ndef(), m_client->tsrc(), far::blocksIn(horizon->content));
	const terragen::WorldCode code = terragen::WorldCode::decode(horizon->code);
	m_year = code.year;
	return { *atlas, *horizon, std::move(palette), std::make_shared<const terragen::System>(terragen::SystemSpec::forWorld(code)), body };
}

std::optional<std::string> Session::showWorld()
{
	try {
		m_planetarium.show(world(bodyHere()));
	} catch (const Refused &e) {
		return std::string(e.what());
	}
	return std::nullopt;
}

Session::~Session()
{
	for (const char *verb : VERBS)
		m_shell.off(verb);
	m_planetarium.serve(nullptr); // its bodies were this session's
}

} // namespace shell
