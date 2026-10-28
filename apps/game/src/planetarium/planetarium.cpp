// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "planetarium/planetarium.h"
#include "constants.h"
#include "log.h"
#include "exceptions.h"
#include <IEventReceiver.h>
#include <IVideoDriver.h>
#include <SMaterial.h>
#include <cmath>
#include <cstdlib>

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

// How far out the camera may go: to see the whole system round the body.
static double farthest(const Planetarium::Opts &o)
{
	if (!o.system)
		throw BaseException("planetarium: a body needs its system");
	const double k = o.atlas.atlas().params().radius / o.system->radius(o.system->playable());
	double out = 0;
	const terragen::System &s = *o.system;
	const auto home = s.position(s.playable(), 0);
	for (size_t i = 0; i < s.spec().bodies.size(); i++) {
		const auto p = s.position(i, 0);
		out = std::max(out, std::hypot(p.x - home.x, p.y - home.y, p.z - home.z) * k);
	}
	return std::max(out * 1.5, 6 * seaOf(o));
}

// A press let go within this many pixels of where it was is a click, not a drag.
static constexpr s32 CLICK = 4;

Planetarium::Planetarium(Opts opts) :
	m_palette(std::move(opts.palette)),
	m_atlas(opts.atlas.atlas()),
	m_sea(seaOf(opts)),
	m_tiles(opts.atlas, opts.horizon, m_palette, buildThreads()),
	m_camera(m_sea, farthest(opts)),
	m_seams(opts.atlas.atlas().params(), m_sea),
	m_markers(m_sea),
	m_system({ opts.system, opts.system->playable(), opts.atlas.atlas().params().radius / opts.system->radius(opts.system->playable()) }),
	m_target(opts.system->playable())
{
}

void Planetarium::focus(double latitude, double longitude, std::optional<double> altitude)
{
	m_camera.focus(latitude, longitude + (m_target == m_system.system().playable() ? m_spin : 0), altitude);
}

void Planetarium::day(double days, std::optional<double> timeOfDay)
{
	m_day = days;
	const terragen::System &s = m_system.system();
	if (timeOfDay) {
		// longitude 0 faces the star at noon: the star's way round the body's axis, less the hours to noon
		const terragen::System::Vec star = m_system.starCentre(days);
		m_spin = std::atan2(star.z, star.x) + 2 * M_PI * (*timeOfDay - 0.5);
	} else {
		const double hours = s.spec().bodies[s.playable()].causes.day / 65536.0;
		m_spin = 2 * M_PI * std::fmod(days * 24 / hours, 1.0);
	}
}

void Planetarium::layer(std::optional<Layer> layer)
{
	if (!layer) {
		m_layer.reset();
		m_tiles.tint(nullptr);
		return;
	}
	if (layer->width == 0 || layer->height == 0 || layer->rgba.size() != (size_t)layer->width * layer->height * 4)
		throw BaseException("planetarium: a layer's pixels don't fill its size");
	m_layer = std::make_shared<const Layer>(std::move(*layer));
	const std::shared_ptr<const Layer> image = m_layer;
	m_tiles.tint(std::make_shared<const far::Tiles::Tint>([image](video::SColor own, const far::Tiling::Vec &at) {
		// where it is round the body, as the preview lays it out
		const double r = std::sqrt(at.x * at.x + at.y * at.y + at.z * at.z);
		const double lat = std::asin(std::clamp(at.y / r, -1.0, 1.0)), lon = std::atan2(at.z, at.x);
		const u32 x = std::min(image->width - 1, (u32)((lon / (2 * M_PI) + 0.5) * image->width));
		const u32 y = std::min(image->height - 1, (u32)((0.5 - lat / M_PI) * image->height));
		const u8 *p = &image->rgba[((size_t)y * image->width + x) * 4];
		// the land's own light and shade, so its relief still shows
		const double shade = 0.7 + 0.3 * std::max({ own.getRed(), own.getGreen(), own.getBlue() }) / 255.0;
		return video::SColor(255, (u32)(p[0] * shade), (u32)(p[1] * shade), (u32)(p[2] * shade));
	}));
}

void Planetarium::keep(const Planetarium &was)
{
	m_day = was.m_day;
	m_spin = was.m_spin;
	if (was.m_target < m_system.system().spec().bodies.size())
		focusBody(was.m_target);
	m_camera = was.m_camera;
}

OrbitCamera Planetarium::cameraFor(size_t body) const
{
	if (body >= m_system.system().spec().bodies.size())
		throw BaseException("planetarium: no body " + std::to_string(body) + " in the system");
	const double sea = body == m_system.system().playable() ? m_sea : m_system.radius(body);
	return OrbitCamera(sea, std::max(m_system.extent() * 1.5, 6 * sea));
}

void Planetarium::focusBody(size_t body)
{
	if (body == m_target && !m_flight)
		return;
	m_flight.reset();
	m_camera = cameraFor(body);
	m_target = body;
}

void Planetarium::fly(const Flight &f)
{
	// where it ends: round the body, as asked, the rest as the camera is (or would be)
	OrbitCamera end = f.body == m_target ? m_camera : cameraFor(f.body);
	const bool playable = f.body == m_system.system().playable();
	const double sea = playable ? m_sea : m_system.radius(f.body);
	const std::optional<double> altitude = f.radii ? std::optional<double>(*f.radii * sea) : f.altitude;
	if (f.latitude)
		end.focus(*f.latitude, *f.longitude + (playable ? m_spin : 0), altitude);
	else if (altitude)
		end.focus(end.latitude(), end.longitude(), altitude);
	if (f.seconds <= 0) {
		m_flight.reset();
		m_target = f.body;
		m_camera = end;
		return;
	}
	// from where it is now (mid-flight or not), from the body it's round
	const Pose now = pose();
	const size_t from = m_flight ? m_flight->from : m_target;
	const OrbitCamera::Vec c = centreOf(from);
	m_flight = Flying{ from, { now.at.x - c.x, now.at.y - c.y, now.at.z - c.z }, f.body, end, f.seconds };
}

OrbitCamera::Vec Planetarium::centreOf(size_t body) const
{
	if (body == m_system.system().playable())
		return { 0, 0, 0 };
	const terragen::System::Vec v = m_system.centre(body, m_day);
	return { v.x, v.y, v.z };
}

Planetarium::Pose Planetarium::pose() const
{
	if (!m_flight) {
		const OrbitCamera::Vec c = centreOf(m_target), p = m_camera.position();
		return { { c.x + p.x, c.y + p.y, c.z + p.z }, c };
	}
	const Flying &f = *m_flight;
	const double x = std::clamp(f.t / f.seconds, 0.0, 1.0), e = x * x * (3 - 2 * x); // eased
	const OrbitCamera::Vec c0 = centreOf(f.from), c1 = centreOf(f.to), o0 = f.start, o1 = f.end.position();
	const auto len = [](const OrbitCamera::Vec &v) { return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z); };
	const OrbitCamera::Vec look{ c0.x + (c1.x - c0.x) * e, c0.y + (c1.y - c0.y) * e, c0.z + (c1.z - c0.z) * e };
	// the way out from what it looks at: turned from the start's to the end's
	const double l0 = len(o0), l1 = len(o1);
	OrbitCamera::Vec d{ o0.x / l0 * (1 - e) + o1.x / l1 * e, o0.y / l0 * (1 - e) + o1.y / l1 * e,
		o0.z / l0 * (1 - e) + o1.z / l1 * e };
	double dl = len(d);
	if (dl < 1e-9)
		d = { 0, 1, 0 }, dl = 1;
	// how far out: between the two, and up to see both bodies on the way
	const OrbitCamera::Vec hop{ c1.x - c0.x, c1.y - c0.y, c1.z - c0.z };
	const double out = std::max(std::exp(std::log(l0) * (1 - e) + std::log(l1) * e), len(hop) * 0.7 * std::sin(M_PI * e));
	return { { look.x + d.x / dl * out, look.y + d.y / dl * out, look.z + d.z / dl * out }, look };
}

core::matrix4 Planetarium::view() const
{
	if (!m_flight)
		return m_camera.view();
	const Pose p = pose();
	core::vector3df ahead((f32)(p.look.x - p.at.x), (f32)(p.look.y - p.at.y), (f32)(p.look.z - p.at.z));
	ahead.normalize();
	core::vector3df up(0, 1, 0);
	if (std::abs(ahead.dotProduct(up)) > 0.99f)
		up = core::vector3df(0, 0, 1);
	core::matrix4 m;
	m.buildCameraLookAtMatrixLH(core::vector3df(0), ahead, up);
	return m;
}

OrbitCamera::Vec Planetarium::eye() const
{
	return pose().at;
}

std::optional<Planetarium::Pick> Planetarium::takeClick()
{
	if (!m_click)
		return std::nullopt;
	const v2s32 at = *m_click;
	m_click.reset();
	return pick(at.X, at.Y);
}

std::optional<Planetarium::Pick> Planetarium::pick(double x, double y) const
{
	if (m_flight || m_target != m_system.system().playable())
		return std::nullopt; // nothing to pick on another body (yet), nor mid-flight
	// the ray from the camera through the pixel, onto the sea's sphere
	const OrbitCamera::Vec o = m_camera.position(),
			d = m_camera.ray(x - m_corner.X, y - m_corner.Y, m_screen.X, m_screen.Y, m_shift);
	const double b = o.x * d.x + o.y * d.y + o.z * d.z;
	const double c = o.x * o.x + o.y * o.y + o.z * o.z - m_sea * m_sea;
	const double disc = b * b - c;
	if (disc < 0)
		return std::nullopt; // off the body
	const double t = -b - std::sqrt(disc);
	if (t < 0)
		return std::nullopt;
	// where it meets the body, in its own (unspun) frame
	const OrbitCamera::Vec hit = OrbitCamera::turn({ o.x + t * d.x, o.y + t * d.y, o.z + t * d.z }, -m_spin);
	const terragen::Atlas::Vec p{ hit.x, hit.y, hit.z };
	const double r = std::sqrt(p.x * p.x + p.y * p.y + p.z * p.z);
	// the cell at the sea there: just under the sea's radius, layer 0
	const double inside = (r - 0.5) / r;
	return Pick{ std::asin(p.y / r), std::atan2(p.z, p.x), m_atlas.locate({ p.x * inside, p.y * inside, p.z * inside }) };
}

bool Planetarium::handle(const SEvent &event)
{
	if (event.EventType != EET_MOUSE_INPUT_EVENT)
		return false;
	const SEvent::SMouseInput &m = event.MouseInput;
	const bool inside = m.X >= m_corner.X && m.Y >= m_corner.Y && m.X < m_corner.X + (s32)m_screen.X &&
			m.Y < m_corner.Y + (s32)m_screen.Y;
	if (!inside && !m_dragging)
		return false; // the page's
	switch (m.Event) {
	case EMIE_LMOUSE_PRESSED_DOWN:
		m_dragging = true;
		m_x = m_downX = m.X;
		m_y = m_downY = m.Y;
		return true;
	case EMIE_LMOUSE_LEFT_UP:
		m_dragging = false;
		if (std::abs(m.X - m_downX) <= CLICK && std::abs(m.Y - m_downY) <= CLICK)
			m_click = v2s32(m.X, m.Y);
		return true;
	case EMIE_MOUSE_MOVED:
		if (m_dragging && !m_flight) { // a flight has the camera
			m_camera.drag(m.X - m_x, m.Y - m_y);
			m_x = m.X;
			m_y = m.Y;
		}
		return true;
	case EMIE_MOUSE_WHEEL:
		if (!m_flight)
			m_camera.zoom(m.Wheel);
		return true;
	default:
		return true;
	}
}

void Planetarium::update(v2u32 screen)
{
	m_screen = screen;
	const auto now = std::chrono::steady_clock::now();
	const double dt = m_last.time_since_epoch().count() ? std::chrono::duration<double>(now - m_last).count() : 0;
	m_last = now;
	if (m_flight && (m_flight->t += std::min(dt, 0.1)) >= m_flight->seconds) {
		// landed: round the body it went to
		m_target = m_flight->to;
		m_camera = m_flight->end;
		m_flight.reset();
	}
	const auto tilesFrom = std::chrono::steady_clock::now();
	m_tiles.update(OrbitCamera::eyeAt(OrbitCamera::turn(eye(), -m_spin), m_sea, (float)screen.Y));
	m_worstTiles = std::max(m_worstTiles, std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - tilesFrom).count());
	if (++m_frame % 120 == 0) {
		infostream << "planetarium: " << m_tiles.drawn().size() << " tiles drawn, " << m_tiles.pending() << " to build, "
				<< (int)m_camera.altitude() << " blocks up; worst frame " << m_worstFrame << " ms (tiles " << m_worstTiles << " ms)"
				<< std::endl;
		m_worstFrame = m_worstTiles = 0;
	}
}

void Planetarium::draw(video::IVideoDriver *driver, core::recti view)
{
	const auto frameFrom = std::chrono::steady_clock::now();
	struct Timed // the frame's time, kept as the worst in the log's window, however it ends
	{
		std::chrono::steady_clock::time_point from;
		double &worst;
		~Timed() { worst = std::max(worst, std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - from).count()); }
	} timed{ frameFrom, m_worstFrame };
	m_corner = view.UpperLeftCorner;
	update(v2u32(std::max(1, view.getWidth()), std::max(1, view.getHeight())));
	const core::dimension2du window = driver->getScreenSize();
	driver->draw2DRectangle(video::SColor(255, 0, 0, 0), view); // space
	driver->setViewPort(view);
	const core::dimension2du screen(m_screen.X, m_screen.Y);

	const float aspect = (float)screen.Width / std::max(1u, screen.Height);
	const OrbitCamera::Vec eye = this->eye();
	const bool round = !m_flight && m_target == m_system.system().playable();
	// the body's own frame (its tiles', its layers'): where the camera is in it, and the view turned with it
	const OrbitCamera::Vec ground = OrbitCamera::turn(eye, -m_spin);
	const core::matrix4 look = this->view(), spun = OrbitCamera::turned(look, m_spin);
	// the system round it, far to near; the body its tiles, in its turn
	m_system.draw(driver, SystemLayer::Camera{ { eye.x, eye.y, eye.z }, look, OrbitCamera::FOV, aspect, m_shift, screen.Height }, m_day, [&](float near, float far) {
		// round it: the camera's own range, tight to the ground; from afar, the layer's
		if (round)
			driver->setTransform(video::ETS_PROJECTION, m_camera.projection(aspect, m_shift));
		else {
			driver->setTransform(video::ETS_PROJECTION, OrbitCamera::perspective(aspect, near, far, m_shift));
		}
		driver->setTransform(video::ETS_VIEW, spun);
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
			world.setTranslation(v3f((float)(tile.middle.x - ground.x), (float)(tile.middle.y - ground.y),
					(float)(tile.middle.z - ground.z)) * BS);
			driver->setTransform(video::ETS_WORLD, world);
			for (const auto &part : tile.parts)
				driver->drawMeshBuffer(part.get());
		}
		if (m_seamsShown)
			m_seams.draw(driver, ground);
		m_markers.draw(driver, ground, m_camera.altitude());
	});
	driver->setViewPort(core::recti(0, 0, window.Width, window.Height)); // the page draws over the whole window
}

} // namespace planetarium
