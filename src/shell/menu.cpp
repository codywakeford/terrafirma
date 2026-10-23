// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "menu.h"
#include "lab.h"
#include "system.h"
#include "orbit.h"
#include "bridge.h"
#include "shell.h"
#include "worlds.h"
#include "terragen/preview.h"
#include "weather/weather.h"
#include "filesys.h"
#include "util/base64.h"
#include <optional>
#include "exceptions.h"
#include "client/renderingengine.h"
#include "porting.h"
#include <ICursorControl.h>
#include <IVideoDriver.h>
#include <IrrlichtDevice.h>
#include <json/json.h>

namespace shell {

static const char *const VERBS[] = {
	"worlds.list", "worlds.create", "worlds.delete", "play", "lab.list", "lab.enter", "quit", "worldgen.preview", "system.preview", "matter.list", "matter.state", "matter.react", "weather.preview",
};

static Json::Value worldJson(const WorldSpec &world)
{
	Json::Value out;
	out["name"] = world.name;
	out["path"] = world.path;
	return out;
}

static std::string bytes(const void *data, size_t size)
{
	return base64_encode(std::string_view(static_cast<const char *>(data), size));
}

/*
	The create screen's preview, from terragen itself: the map you see is the
	world you get. Fields go as base64 of their raw little-endian arrays (see
	terragen/preview.h for the scales), with the biome names alongside.
*/
static Json::Value previewJson(const Json::Value &intent)
{
	if (!intent["code"].isString())
		throw Refused("worldgen.preview needs a world code");
	int resolution = intent["resolution"].isInt() ? intent["resolution"].asInt() : 0;
	if (resolution < 16 || resolution > 512)
		throw Refused("preview resolution must be 16..512");

	terragen::WorldCode code = readWorldCode(intent["code"].asString());
	std::optional<terragen::fix> date;
	if (intent["date"].isNumeric()) {
		double d = intent["date"].asDouble();
		if (d < 0 || d >= 1)
			throw Refused("preview date must be in [0, 1)");
		date = (terragen::fix)(d * terragen::ONE);
	}

	terragen::World world(code);
	terragen::Preview p = terragen::preview(world, date, resolution);

	Json::Value out;
	out["resolution"] = p.resolution;
	out["elevation"] = bytes(p.elevation.data(), p.elevation.size() * sizeof(int16_t));
	out["heat"] = bytes(p.heat.data(), p.heat.size());
	out["wet"] = bytes(p.wet.data(), p.wet.size());
	out["biome"] = bytes(p.biome.data(), p.biome.size());
	out["lake"] = bytes(p.lake.data(), p.lake.size());
	out["grid"] = p.grid;
	out["rivers"] = bytes(p.rivers.data(), p.rivers.size());
	out["province"] = bytes(p.province.data(), p.province.size());
	out["plate"] = bytes(p.plate.data(), p.plate.size());
	out["boundary"] = bytes(p.boundary.data(), p.boundary.size());
	out["volcanoes"] = bytes(p.volcanoes.data(), p.volcanoes.size());
	Json::Value names(Json::arrayValue);
	for (const char *name : terragen::BIOME_NAMES)
		names.append(name);
	out["biomes"] = names;
	return out;
}

/*
	A settled year of the world's weather for the create screen: `snapshots`
	maps at evenly spaced dates (from the northern spring equinox), on the
	weather's own grid. Each field holds every snapshot in turn, as base64:
	rain uint8 (√(mm/h) × 100), cloud uint8 (× 255), temperature int8 (°C),
	wind east and north int8 (m/s), pressure int8 (hPa from the mean).
*/
static Json::Value weatherJson(const Json::Value &intent)
{
	if (!intent["code"].isString())
		throw Refused("weather.preview needs a world code");
	int snapshots = intent["snapshots"].isInt() ? intent["snapshots"].asInt() : 0;
	if (snapshots < 1 || snapshots > 64)
		throw Refused("weather.preview: snapshots must be 1..64");
	static const matter::Matter table = matter::Matter::load(porting::path_share + DIR_DELIM "matter");
	terragen::World world(readWorldCode(intent["code"].asString()));
	std::vector<fluid::Layer::Fields> year = weather::Weather::year(world, table, snapshots);

	std::string rain, cloud, temperature, east, north, pressure;
	auto byte = [](double v, double lo, double hi) { return (char)(int8_t)std::lround(std::clamp(v, lo, hi)); };
	for (const fluid::Layer::Fields &f : year)
		for (size_t i = 0; i < f.t.size(); i++) {
			rain += (char)(uint8_t)std::lround(std::min(255.0, std::sqrt(f.rain[i]) * 100));
			cloud += (char)(uint8_t)std::lround(f.cloud[i] * 255);
			temperature += byte(f.t[i] - 273.15, -128, 127);
			east += byte(f.u[i], -128, 127);
			north += byte(f.v[i], -128, 127);
			pressure += byte(f.pressure[i] / 100, -128, 127);
		}
	Json::Value out;
	out["resolution"] = 64;
	out["snapshots"] = snapshots;
	out["rain"] = bytes(rain.data(), rain.size());
	out["cloud"] = bytes(cloud.data(), cloud.size());
	out["temperature"] = bytes(temperature.data(), temperature.size());
	out["east"] = bytes(east.data(), east.size());
	out["north"] = bytes(north.data(), north.size());
	out["pressure"] = bytes(pressure.data(), pressure.size());
	return out;
}

static std::string stringField(const Json::Value &intent, const char *field)
{
	if (!intent[field].isString())
		throw Refused(std::string("missing string field '") + field + "'");
	return intent[field].asString();
}

Menu::Menu(const Opts &opts) :
	m_shell(opts.shell), m_engine(opts.engine), m_worlds(opts.worlds), m_lab(opts.lab)
{
	m_shell.on("worlds.list", [this](const Json::Value &) {
		Json::Value out(Json::arrayValue);
		for (const auto &world : m_worlds.list())
			out.append(worldJson(world));
		return out;
	});
	m_shell.on("worlds.create", [this](const Json::Value &intent) {
		return worldJson(m_worlds.create(stringField(intent, "name"), stringField(intent, "code"),
				intent["settings"]));
	});
	m_shell.on("worlds.delete", [this](const Json::Value &intent) {
		m_worlds.remove(stringField(intent, "path"));
		return Json::Value();
	});
	m_shell.on("play", [this](const Json::Value &intent) {
		m_choice = m_worlds.find(stringField(intent, "path"));
		m_worlds.select(*m_choice);
		return Json::Value();
	});
	m_shell.on("lab.list", [this](const Json::Value &) {
		return m_lab.list();
	});
	m_shell.on("lab.enter", [this](const Json::Value &intent) {
		m_choice = m_worlds.lab(m_lab.find(stringField(intent, "id")));
		m_worlds.select(*m_choice);
		return Json::Value();
	});
	// ~150 ms when the land changes (the water simulation): off the main
	// thread, so the menu keeps drawing while a slider is dragged.
	m_shell.onAsync("worldgen.preview", [](const Json::Value &intent) {
		return previewJson(intent);
	});
	// about a second: a year of weather to settle, then a year to sample
	m_shell.onAsync("weather.preview", [](const Json::Value &intent) {
		try {
			return weatherJson(intent);
		} catch (const fluid::FluidError &e) {
			throw Refused(e.what());
		}
	});
	// microseconds: a few hundred fixed-point steps per body
	m_shell.on("system.preview", [](const Json::Value &intent) {
		return systemPreview(intent);
	});
	m_shell.on("matter.list", [this](const Json::Value &) {
		return m_matter.list();
	});
	m_shell.on("matter.state", [this](const Json::Value &intent) {
		return m_matter.state(intent);
	});
	m_shell.on("matter.react", [this](const Json::Value &intent) {
		return m_matter.react(intent);
	});
	m_shell.on("quit", [this](const Json::Value &) {
		m_quit = true;
		return Json::Value();
	});

}

Menu::~Menu()
{
	for (const char *verb : VERBS)
		m_shell.off(verb);
}

std::optional<WorldSpec> Menu::run(const std::string &error)
{
	if (!error.empty()) {
		Json::Value message;
		message["kind"] = "error";
		message["message"] = error;
		m_shell.send(message);
	}

	auto *device = m_engine->get_raw_device();
	auto *cursor = device->getCursorControl();
	cursor->setVisible(true);
	cursor->setRelativeMode(false);

	video::IVideoDriver *driver = m_engine->get_video_driver();
	Orbit orbit(driver);
	u64 scene_start = porting::getTimeMs();
	volatile auto *kill = porting::signal_handler_killstatus();
	const u64 frame_ms = 1000 / 60;

	while (m_engine->run() && !*kill && !m_choice && !m_quit) {
		u64 start = porting::getTimeMs();
		driver->beginScene(true, true, video::SColor(255, 12, 14, 12));
		orbit.draw((porting::getTimeMs() - scene_start) / 1000.0f);
		m_shell.frame();
		driver->endScene();
		u64 spent = porting::getTimeMs() - start;
		if (spent < frame_ms)
			sleep_ms(frame_ms - spent);
	}
	return m_choice;
}

} // namespace shell
