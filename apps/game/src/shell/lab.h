// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <json/json.h>
#include <string>

namespace shell {

/*
	The Lab's scenarios as the menu sees them, before any game Lua runs: every
	`lab.json` among the game's mods (games/terrafirma/mods/tf_lab declares
	the format and checks it in full when a scenario's world loads). Read
	fresh on every call, so edits show without a restart.
*/
class Lab
{
public:
	explicit Lab(const std::string &mods_dir) : m_mods_dir(mods_dir) {}

	// Every scenario, in mod then file order, each with its mod's name.
	Json::Value list() const;
	// One scenario by id; throws Refused if there's none.
	Json::Value find(const std::string &id) const;

private:
	std::string m_mods_dir;
};

} // namespace shell
