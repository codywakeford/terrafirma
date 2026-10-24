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
};

static const Option &find(const std::string &key)
{
	for (const Option &option : OPTIONS) {
		if (key == option.key)
			return option;
	}
	throw Refused("\"" + key + "\" is not a setting the menu can change");
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
	if (!g_settings_path.empty() && !g_settings->updateConfigFile(g_settings_path.c_str()))
		throw BaseException("shell: could not save settings to " + g_settings_path);
	infostream << "shell: set " << keys.size() << " setting(s)" << std::endl;
}

} // namespace shell
