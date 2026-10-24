// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "state.h"
#include "items.h"
#include "shell.h"
#include "client/client.h"
#include "client/content_cao.h"
#include "client/localplayer.h"
#include "convert_json.h"
#include "hud_element.h"
#include "inventory.h"

namespace shell {

void State::update(const HudView &view)
{
	LocalPlayer *player = m_opts.client->getEnv().getLocalPlayer();
	// The player's object (and with it max HP/breath) arrives a moment after
	// the session starts; until then there is nothing true to report.
	if (!player || !player->getCAO())
		return;

	send("hud", hud(*player, view));
	send("player", this->player(*player));
	send("hotbar", hotbar(*player));
	Json::Value inv = inventory(*player);
	sendNewItems(); // before the inventory that references them
	send("inventory", inv);
}

void State::forget()
{
	m_sent.clear();
	m_sent_items.clear();
	m_new_items = Json::objectValue;
}

Json::Value State::player(LocalPlayer &player) const
{
	const ObjectProperties &props = player.getCAO()->getProperties();
	Json::Value out;
	out["hp"] = player.hp;
	out["hpMax"] = props.hp_max;
	out["breath"] = player.getBreath();
	out["breathMax"] = props.breath_max;
	return out;
}

Json::Value State::hud(LocalPlayer &player, const HudView &view) const
{
	// The server can hide parts of the HUD per player (hud_set_flags).
	Json::Value out;
	out["visible"] = view.visible;
	out["crosshair"] = view.crosshair;
	out["hotbar"] = (player.hud_flags & HUD_FLAG_HOTBAR_VISIBLE) != 0;
	out["health"] = (player.hud_flags & HUD_FLAG_HEALTHBAR_VISIBLE) != 0;
	out["breath"] = (player.hud_flags & HUD_FLAG_BREATHBAR_VISIBLE) != 0;
	return out;
}

Json::Value State::hotbar(LocalPlayer &player) const
{
	Json::Value out;
	out["size"] = player.getMaxHotbarItemcount();
	out["selected"] = player.getWieldIndex();
	return out;
}

Json::Value State::inventory(LocalPlayer &player)
{
	Json::Value lists(Json::objectValue);
	for (const InventoryList *list : player.inventory.getLists()) {
		Json::Value stacks(Json::arrayValue);
		for (u32 i = 0; i < list->getSize(); i++) {
			const ItemStack &stack = list->getItem(i);
			if (stack.empty()) {
				stacks.append(Json::nullValue);
				continue;
			}
			// The catalogue may have generated this icon before the player ever
			// held the item. Still send its metadata to the page once.
			if (m_sent_items.insert(stack.name).second)
				m_new_items[stack.name] = m_opts.items.describe(stack.name);
			Json::Value s;
			s["item"] = stack.name;
			s["count"] = stack.count;
			s["wear"] = stack.wear;
			stacks.append(s);
		}
		Json::Value l;
		l["width"] = list->getWidth();
		l["stacks"] = stacks;
		lists[list->getName()] = l;
	}
	return lists;
}

void State::sendNewItems()
{
	if (m_new_items.empty())
		return;
	Json::Value message;
	message["kind"] = "items";
	message["items"] = m_new_items;
	m_opts.shell.send(message);
	m_new_items = Json::objectValue;
}

void State::send(const char *section, const Json::Value &value)
{
	std::string encoded = fastWriteJson(value);
	std::string &last = m_sent[section];
	if (encoded == last)
		return;
	last = encoded;

	Json::Value message;
	message["kind"] = "state";
	message["section"] = section;
	message["value"] = value;
	m_opts.shell.send(message);
}

} // namespace shell
