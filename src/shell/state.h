// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <json/json.h>
#include <string>
#include <unordered_map>

class Client;
class LocalPlayer;

namespace shell {

class Shell;
class Items;

// What the engine decided about the HUD this frame (F1, camera mode, touch).
struct HudView
{
	bool visible;
	bool crosshair;
};

/*
	The game as the HUD sees it, read from the local player once per frame.
	Each section (player, hotbar, inventory) is sent when its content differs
	from what the page last received — compared by value, not dirty flags,
	which the engine resets for its own purposes.
*/
class State
{
public:
	struct Opts {
		Shell &shell;
		Client *client;
		Items &items;
	};

	explicit State(const Opts &opts) : m_opts(opts) {}

	void update(const HudView &view);

private:
	Json::Value player(LocalPlayer &player) const;
	Json::Value hud(LocalPlayer &player, const HudView &view) const;
	Json::Value hotbar(LocalPlayer &player) const;
	Json::Value inventory(LocalPlayer &player);
	void sendNewItems();
	void send(const char *section, const Json::Value &value);

	Opts m_opts;
	std::unordered_map<std::string, std::string> m_sent;
	Json::Value m_new_items = Json::objectValue;
};

} // namespace shell
