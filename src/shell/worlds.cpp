// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "worlds.h"
#include "bridge.h"
#include "exceptions.h"
#include "filesys.h"
#include "log.h"
#include "porting.h"
#include "settings.h"
#include "terragen/code.h"
#include "util/string.h"

namespace shell {

std::vector<WorldSpec> Worlds::list() const
{
	std::vector<WorldSpec> ours;
	for (const auto &world : getAvailableWorlds()) {
		if (world.gameid == m_gameid)
			ours.push_back(world);
	}
	return ours;
}

/*
	Which world settings the page may write at creation. The engine's own
	creative_mode / enable_damage take effect now; tf_* keys are this game's
	rules and generation settings, read by its Lua (and terragen) as those
	systems exist. Values are single words or numbers.
*/
static void validateSetting(const std::string &key, const Json::Value &value)
{
	bool engine = key == "creative_mode" || key == "enable_damage";
	bool ours = key.size() > 3 && key.rfind("tf_", 0) == 0 &&
			key.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_") == std::string::npos;
	if (!engine && !ours)
		throw Refused("\"" + key + "\" is not a world setting this menu may write");
	if (!value.isBool() && !value.isNumeric() && !value.isString())
		throw Refused(key + " must be a word, number or true/false");
	if (value.isString() && value.asString().find_first_of("\n\r\"") != std::string::npos)
		throw Refused(key + " has characters a setting can't hold");
}

terragen::WorldCode readWorldCode(const std::string &code)
{
	try {
		return terragen::WorldCode::decode(code);
	} catch (const terragen::CodeError &e) {
		throw Refused(e.what());
	}
}

static std::string settingText(const Json::Value &value)
{
	if (value.isBool())
		return value.asBool() ? "true" : "false";
	if (value.isString())
		return value.asString();
	return value.asString(); // numbers: jsoncpp's own formatting
}

WorldSpec Worlds::create(const std::string &name, const std::string &code,
		const Json::Value &settings) const
{
	const terragen::WorldCode world_code = readWorldCode(code);
	if (!settings.isObject())
		throw Refused("world settings must be an object");
	for (const auto &key : settings.getMemberNames())
		validateSetting(key, settings[key]);

	std::string trimmed(trim(std::string_view(name)));
	if (trimmed.empty())
		throw Refused("A world needs a name.");

	SubgameSpec game = findSubgame(m_gameid);
	if (!game.isValid())
		throw BaseException("shell: game '" + m_gameid + "' is not installed");

	std::string path = porting::path_user + DIR_DELIM "worlds" DIR_DELIM +
			sanitizeDirName(trimmed, "world_");
	if (fs::PathExists(path))
		throw Refused("A world called \"" + trimmed + "\" already exists.");

	loadGameConfAndInitWorld(path, trimmed, game, true);
	// The map generator's settings: terragen, this code, and the engine's own
	// seed (ores, decorations) tied to the code's seed.
	{
		Settings meta("[end_of_params]");
		meta.set("mg_name", "terra");
		meta.set("mgterra_code", code);
		meta.set("seed", std::to_string(world_code.seed_hash));
		if (!meta.updateConfigFile((path + DIR_DELIM "map_meta.txt").c_str()))
			throw BaseException("shell: could not write the map settings for " + path);
	}
	if (!settings.empty()) {
		std::string conf = path + DIR_DELIM "world.mt";
		Settings world;
		if (!world.readConfigFile(conf.c_str()))
			throw BaseException("shell: could not read " + conf);
		for (const auto &key : settings.getMemberNames())
			world.set(key, settingText(settings[key]));
		if (!world.updateConfigFile(conf.c_str()))
			throw BaseException("shell: could not write " + conf);
	}
	actionstream << "shell: created world " << trimmed << " at " << path << std::endl;
	return WorldSpec(path, trimmed, m_gameid);
}

WorldSpec Worlds::lab(const Json::Value &scenario) const
{
	const Json::Value &mapgen = scenario["world"]["mapgen"];
	if (!mapgen.isNull() && !mapgen.isString())
		throw BaseException("shell: Lab scenario " + scenario["id"].asString() + ": world.mapgen must be a name");
	SubgameSpec game = findSubgame(m_gameid);
	if (!game.isValid())
		throw BaseException("shell: game '" + m_gameid + "' is not installed");

	std::string path = porting::path_user + DIR_DELIM "lab";
	if (fs::PathExists(path) && !fs::RecursiveDelete(path))
		throw BaseException("shell: could not clear the old Lab at " + path);
	loadGameConfAndInitWorld(path, "Lab", game, true);
	// tf_lab lays the floor and builds the scenario
	{
		Settings meta("[end_of_params]");
		meta.set("mg_name", mapgen.isNull() ? "singlenode" : mapgen.asString());
		meta.set("tf_lab", scenario["id"].asString());
		if (!meta.updateConfigFile((path + DIR_DELIM "map_meta.txt").c_str()))
			throw BaseException("shell: could not write the map settings for " + path);
	}
	{
		std::string conf = path + DIR_DELIM "world.mt";
		Settings world;
		if (!world.readConfigFile(conf.c_str()))
			throw BaseException("shell: could not read " + conf);
		world.set("creative_mode", "true");
		world.set("enable_damage", "false");
		if (!world.updateConfigFile(conf.c_str()))
			throw BaseException("shell: could not write " + conf);
	}
	actionstream << "shell: built the Lab for " << scenario["id"].asString() << " at " << path << std::endl;
	return WorldSpec(path, "Lab", m_gameid);
}

void Worlds::remove(const std::string &path) const
{
	WorldSpec world = find(path);
	if (!fs::RecursiveDelete(world.path))
		throw BaseException("shell: could not delete world at " + world.path);
	actionstream << "shell: deleted world " << world.name << " at " << world.path << std::endl;
}

void Worlds::select(const WorldSpec &world) const
{
	std::string conf = world.path + DIR_DELIM "world.mt";
	Settings settings;
	if (!settings.readConfigFile(conf.c_str()))
		throw BaseException("shell: could not read " + conf);
	for (const char *key : { "creative_mode", "enable_damage" }) {
		if (settings.exists(key))
			g_settings->setBool(key, settings.getBool(key));
	}
}

WorldSpec Worlds::find(const std::string &path) const
{
	// Only paths we listed: the page never names an arbitrary directory.
	for (const auto &world : list()) {
		if (world.path == path)
			return world;
	}
	throw Refused("That world no longer exists.");
}

} // namespace shell
