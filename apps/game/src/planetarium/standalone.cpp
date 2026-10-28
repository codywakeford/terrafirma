// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "planetarium/standalone.h"
#include "planetarium/planetarium.h"
#include "client/inputhandler.h"
#include "client/renderingengine.h"
#include "exceptions.h"
#include "far/baked.h"
#include "filesys.h"
#include "light.h"
#include "log.h"
#include "settings.h"
#include "mapgen/mapgen.h"
#include "porting.h"
#include "terragen/code.h"
#include <IVideoDriver.h>
#include <cmath>
#include <fstream>

namespace planetarium {

void runStandalone(const std::string &code)
{
	const terragen::WorldCode world = terragen::WorldCode::decode(code);
	const std::string path = porting::path_share + DIR_DELIM "games" DIR_DELIM "terrafirma" DIR_DELIM "palette.json";
	std::ifstream in(path);
	if (!in)
		throw BaseException("planetarium: no baked palette at " + path + " (bun stage)");
	Json::Value json;
	in >> json;
	far::Baked baked = far::Baked::read(json);

	// the light curve tiles are lit by, as a client sets it when its server sends none
	set_light_curve(g_settings->getFloat("display_gamma"));

	MyEventReceiver receiver;
	RenderingEngine engine(&receiver);
	engine.get_raw_device()->setWindowCaption(L"Terra Firma — Planetarium");

	// as a new world of the code is laid out (shell/worlds.cpp): as many blocks round as it is
	const planet::Atlas atlas(terragen::Atlas::standard(world.size / (2 * M_PI)));
	const far::Horizon horizon{ code, MapgenParams().water_level, std::move(baked.content) };
	Planetarium view({ atlas, horizon, std::move(baked.palette) });
	bool quit = false;
	receiver.capture = [&](const SEvent &event) {
		if (event.EventType == EET_KEY_INPUT_EVENT) {
			if (event.KeyInput.PressedDown && event.KeyInput.Key == KEY_ESCAPE)
				quit = true;
			return true;
		}
		return view.handle(event);
	};
	infostream << "planetarium: " << code << ", " << atlas.atlas().code() << std::endl;

	video::IVideoDriver *driver = engine.get_video_driver();
	volatile auto *kill = porting::signal_handler_killstatus(); // a signal ends it, as it ends the game
	const u64 frame_ms = 1000 / 60; // as the menu: no faster than the screen
	while (!quit && !*kill && engine.run()) {
		const u64 start = porting::getTimeMs();
		driver->beginScene(true, true, video::SColor(255, 8, 10, 14));
		view.draw(driver);
		driver->endScene();
		const u64 spent = porting::getTimeMs() - start;
		if (spent < frame_ms)
			sleep_ms(frame_ms - spent);
	}
	receiver.capture = nullptr;
}

} // namespace planetarium
