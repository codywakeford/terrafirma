// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "options.h"
#include "bridge.h"
#include "exceptions.h"
#include "log.h"
#include "settings.h"

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
	{ "mouse_sensitivity", Kind::Number, 0.05f, 1.0f },
	{ "sound_volume", Kind::Number, 0, 1 },
	{ "fullscreen", Kind::Flag, 0, 0 },
	{ "vsync", Kind::Flag, 0, 0 },
	{ "enable_dynamic_shadows", Kind::Flag, 0, 0 },
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
	const Option &option = find(key);
	if (option.kind == Kind::Flag) {
		if (!value.isBool())
			throw Refused(key + " takes true or false");
		g_settings->setBool(key, value.asBool());
	} else {
		if (!value.isNumeric())
			throw Refused(key + " takes a number");
		float v = value.asFloat();
		if (v < option.min || v > option.max)
			throw Refused(key + " must be between " + std::to_string(option.min) +
					" and " + std::to_string(option.max));
		g_settings->setFloat(key, v);
	}
	if (!g_settings_path.empty() && !g_settings->updateConfigFile(g_settings_path.c_str()))
		throw BaseException("shell: could not save settings to " + g_settings_path);
	infostream << "shell: set " << key << std::endl;
}

} // namespace shell
