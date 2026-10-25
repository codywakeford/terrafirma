// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "options.h"
#include "bridge.h"
#include "exceptions.h"
#include "log.h"
#include "settings.h"
#include <cmath>
#include <vector>

namespace shell {

enum class Kind { Number, Flag };

struct Option
{
	const char *key;
	Kind kind;
	float min;
	float max;
};

// Labels and grouping are the page's (ui/src/settings/options.ts); ranges are ours.
static const Option OPTIONS[] = {
	{ "viewing_range", Kind::Number, 20, 1000 },
	{ "fov", Kind::Number, 45, 160 },
	{ "fps_max", Kind::Number, 30, 360 },
	{ "fps_max_unfocused", Kind::Number, 5, 60 },
	{ "mouse_sensitivity", Kind::Number, 0.05f, 1.0f },
	{ "sound_volume", Kind::Number, 0, 1 },
	{ "sound_volume_unfocused", Kind::Number, 0, 1 },
	{ "hud_scaling", Kind::Number, 0.5f, 2.0f },
	{ "view_bobbing_amount", Kind::Number, 0, 2 },
	{ "fullscreen", Kind::Flag, 0, 0 },
	{ "vsync", Kind::Flag, 0, 0 },
	{ "enable_dynamic_shadows", Kind::Flag, 0, 0 },
	{ "enable_bloom", Kind::Flag, 0, 0 },
	{ "enable_fog", Kind::Flag, 0, 0 },
	{ "enable_3d_clouds", Kind::Flag, 0, 0 },
	{ "invert_mouse", Kind::Flag, 0, 0 },
	{ "doubletap_jump", Kind::Flag, 0, 0 },
	{ "toggle_sneak_key", Kind::Flag, 0, 0 },
	{ "mute_sound", Kind::Flag, 0, 0 },
	{ "hurt_flash_enabled", Kind::Flag, 0, 0 },
	{ "far_view", Kind::Flag, 0, 0 },
	{ "far_view_detail", Kind::Number, 1, 4 },
	{ "far_view_max", Kind::Number, 2000, 64000 },
	{ "widget_map", Kind::Flag, 0, 0 },
	{ "widget_map_corner", Kind::Number, 0, 3 },
	{ "widget_map_scale", Kind::Number, 1, 3 },
	{ "widget_map_shape", Kind::Number, 0, 1 },
	{ "widget_map_rotate", Kind::Flag, 0, 0 },
	{ "widget_map_zoom", Kind::Number, 1, 5 },
	{ "widget_world_controls", Kind::Flag, 0, 0 },
	{ "widget_world_controls_corner", Kind::Number, 0, 3 },
};

static const Option &find(const std::string &key)
{
	for (const Option &option : OPTIONS) {
		if (key == option.key)
			return option;
	}
	throw Refused("\"" + key + "\" is not a setting the menu can change");
}

static std::vector<std::string> bindingNames()
{
	std::vector<std::string> names;
	for (const std::string &key : Settings::getLayer(SL_DEFAULTS)->getNames()) {
		if (key.rfind("keymap_", 0) == 0)
			names.push_back(key);
	}
	return names;
}

static void checkBinding(const std::string &key)
{
	for (const std::string &name : bindingNames()) {
		if (name == key)
			return;
	}
	throw Refused("unknown key binding " + key);
}

static std::string keyboardToken(const std::string &value)
{
	size_t start = 0;
	while (start <= value.size()) {
		size_t end = value.find('|', start);
		std::string token = value.substr(start, end == std::string::npos ? end : end - start);
		if (token.rfind("SYSTEM_SCANCODE_", 0) == 0)
			return token;
		if (end == std::string::npos)
			break;
		start = end + 1;
	}
	return "";
}

static std::string withKeyboard(const std::string &value, const std::string &keyboard)
{
	std::string result = keyboard;
	size_t start = 0;
	while (start < value.size()) {
		size_t end = value.find('|', start);
		std::string token = value.substr(start, end == std::string::npos ? end : end - start);
		if (!token.empty() && token.rfind("SYSTEM_SCANCODE_", 0) != 0) {
			if (!result.empty())
				result += '|';
			result += token;
		}
		if (end == std::string::npos)
			break;
		start = end + 1;
	}
	return result;
}

static void save()
{
	if (!g_settings_path.empty() && !g_settings->updateConfigFile(g_settings_path.c_str()))
		throw BaseException("shell: could not save settings to " + g_settings_path);
}

Json::Value Options::get() const
{
	Json::Value out(Json::objectValue);
	for (const Option &option : OPTIONS) {
		if (option.kind == Kind::Flag)
			out[option.key] = g_settings->getBool(option.key);
		else
			out[option.key] = g_settings->getFloat(option.key);
	}
	return out;
}

void Options::set(const std::string &key, const Json::Value &value) const
{
	Json::Value values(Json::objectValue);
	values[key] = value;
	apply(values);
}

void Options::apply(const Json::Value &values) const
{
	if (!values.isObject() || values.empty())
		throw Refused("settings.apply needs a non-empty object");
	const auto keys = values.getMemberNames();
	for (const std::string &key : keys) {
		const Option &option = find(key);
		const Json::Value &value = values[key];
		if (option.kind == Kind::Flag) {
			if (!value.isBool())
				throw Refused(key + " takes true or false");
		} else {
			if (!value.isNumeric() || !std::isfinite(value.asDouble()))
				throw Refused(key + " takes a finite number");
			double v = value.asDouble();
			if (v < option.min || v > option.max)
				throw Refused(key + " must be between " + std::to_string(option.min) +
						" and " + std::to_string(option.max));
		}
	}
	for (const std::string &key : keys) {
		const Option &option = find(key);
		if (option.kind == Kind::Flag)
			g_settings->setBool(key, values[key].asBool());
		else
			g_settings->setFloat(key, values[key].asFloat());
	}
	save();
	infostream << "shell: set " << keys.size() << " setting(s)" << std::endl;
}

Json::Value Options::bindings() const
{
	Json::Value out(Json::objectValue);
	for (const std::string &key : bindingNames())
		out[key] = g_settings->get(key);
	return out;
}

void Options::bind(const std::string &key, int scancode, bool replace) const
{
	checkBinding(key);
	// SDL scancodes used by KeyPress. Escape remains the modal's close key.
	if (scancode < 4 || scancode > 231 || scancode == 41)
		throw Refused("that key cannot be assigned");
	const std::string token = "SYSTEM_SCANCODE_" + std::to_string(scancode);
	std::vector<std::string> conflicts;
	for (const std::string &name : bindingNames()) {
		if (name != key && keyboardToken(g_settings->get(name)) == token)
			conflicts.push_back(name);
	}
	if (!conflicts.empty() && !replace)
		throw Refused("key already assigned to " + conflicts.front());
	for (const std::string &name : conflicts)
		g_settings->set(name, withKeyboard(g_settings->get(name), ""));
	g_settings->set(key, withKeyboard(g_settings->get(key), token));
	save();
}

void Options::resetBindings() const
{
	for (const std::string &key : bindingNames())
		g_settings->remove(key);
	save();
}

} // namespace shell
