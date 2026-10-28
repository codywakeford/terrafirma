// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "planetarium/planetarium.h"
#include "constants.h"
#include "log.h"
#include <IEventReceiver.h>
#include <IVideoDriver.h>
#include <SMaterial.h>

namespace planetarium {

// All but the main thread's and the driver's: the Planetarium is all that runs.
static unsigned buildThreads()
{
	const unsigned all = far::Tiles::threadsFor(1);
	return all > 2 ? all - 2 : 1;
}

static double seaOf(const Planetarium::Opts &o)
{
	return o.atlas.atlas().params().radius + o.horizon.water_level + 1;
}

Planetarium::Planetarium(Opts opts) :
	m_palette(std::move(opts.palette)),
	m_tiles(opts.atlas, opts.horizon, m_palette, buildThreads()),
	m_camera(seaOf(opts)),
	m_seams(opts.atlas.atlas().params(), seaOf(opts))
{
}

bool Planetarium::handle(const SEvent &event)
{
	if (event.EventType != EET_MOUSE_INPUT_EVENT)
		return false;
	const SEvent::SMouseInput &m = event.MouseInput;
	switch (m.Event) {
	case EMIE_LMOUSE_PRESSED_DOWN:
		m_dragging = true;
		m_x = m.X;
		m_y = m.Y;
		return true;
	case EMIE_LMOUSE_LEFT_UP:
		m_dragging = false;
		return true;
	case EMIE_MOUSE_MOVED:
		if (m_dragging) {
			m_camera.drag(m.X - m_x, m.Y - m_y);
			m_x = m.X;
			m_y = m.Y;
		}
		return true;
	case EMIE_MOUSE_WHEEL:
		m_camera.zoom(m.Wheel);
		return true;
	default:
		return true;
	}
}

void Planetarium::draw(video::IVideoDriver *driver)
{
	const core::dimension2du screen = driver->getScreenSize();
	m_tiles.update(m_camera.eye((float)screen.Height));
	if (++m_frame % 120 == 0)
		infostream << "planetarium: " << m_tiles.drawn().size() << " tiles drawn, " << m_tiles.pending()
				<< " to build, " << (int)m_camera.altitude() << " blocks up" << std::endl;

	driver->setTransform(video::ETS_PROJECTION, m_camera.projection((float)screen.Width / std::max(1u, screen.Height)));
	driver->setTransform(video::ETS_VIEW, m_camera.view());

	const OrbitCamera::Vec eye = m_camera.position();
	video::SMaterial land;
	land.MaterialType = video::EMT_SOLID;
	land.FogEnable = false;
	land.BackfaceCulling = false;
	// colour only: the mesh's vertex alpha marks its layers (far/mesh.cpp:
	// 0 a liquid's top), and a window can show alpha
	land.ColorMask = video::ECP_RGB;
	driver->setMaterial(land);
	for (const far::Tiles::Drawn &tile : m_tiles.drawn()) {
		core::matrix4 world;
		world.setTranslation(v3f((float)(tile.middle.x - eye.x), (float)(tile.middle.y - eye.y),
				(float)(tile.middle.z - eye.z)) * BS);
		driver->setTransform(video::ETS_WORLD, world);
		for (const auto &part : tile.parts)
			driver->drawMeshBuffer(part.get());
	}
	m_seams.draw(driver, eye);
}

} // namespace planetarium
