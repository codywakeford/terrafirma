// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "far/palette.h"
#include "far/terrain.h"
#include "far/tiles.h"
#include "planet/atlas.h"
#include "planetarium/camera.h"
#include "planetarium/markers.h"
#include "planetarium/seams.h"
#include "planetarium/system.h"
#include <chrono>
#include <memory>

struct SEvent;
namespace video { class IVideoDriver; }

/*
	The Planetarium (planning/sprint-31-planetarium.md): one planet viewer,
	native, drawn under whatever page is shown over it. A body's land is
	far::Tiles for the camera's eye, the same tiles the far view draws in
	game; layers draw over it (the atlas's seams, for now). Hosts make one
	and give it the window: the game (a key), and `luanti --planetarium`
	(a world code alone, from the game's baked palette).
*/
namespace planetarium {

class Planetarium
{
public:
	struct Opts
	{
		planet::Atlas atlas;
		far::Horizon horizon;
		far::Palette palette;
		// the system the body is in: it's its playable body, drawn from its tiles
		std::shared_ptr<const terragen::System> system;
	};

	explicit Planetarium(Opts opts);

	// The mouse: drag to orbit, the wheel to go up and down; a click (a press
	// let go where it was) is kept for takeClick. Only what starts in the last
	// frame's view (a drag may leave it). Whether it was the Planetarium's.
	bool handle(const SEvent &event);

	// A place on the body: where, and the atlas cell at the sea there.
	struct Pick
	{
		double latitude, longitude; // radians
		terragen::Atlas::Cell cell;
	};
	// The last click, once: the place under it, or none off the body.
	std::optional<Pick> takeClick();
	// The place under a window pixel of the last frame's view; none off the body.
	std::optional<Pick> pick(double x, double y) const;

	// Whether the whole body is drawn, if coarsely (its coarse tiles built).
	bool ready() const { return m_tiles.based(); }

	// Over a place on the playable body (its ground's latitude and longitude):
	// the camera goes round to it as the body is turned now.
	void focus(double latitude, double longitude, std::optional<double> altitude);
	// A flight: to a body (its index), over a place on it (latitude and
	// longitude of its ground, radians) and at an altitude (blocks), each
	// kept as it is where not given; eased over `seconds` (0: there now).
	// From afar it climbs to see both bodies, and comes down.
	struct Flight
	{
		size_t body;
		std::optional<double> latitude, longitude, altitude;
		std::optional<double> radii; // or the altitude in the body's radii
		double seconds;
	};
	void fly(const Flight &flight);
	bool flying() const { return m_flight.has_value(); }

	// Where the view's middle is drawn (the free part of the screen: as
	// OrbitCamera::perspective's shift).
	void frame(v2f shift) { m_shift = shift; }

	// A layer over the playable body's land (planning/sprint-37-layers.md): an image
	// round it, equirectangular as the world model's preview is (row 0 the
	// north edge, column 0 longitude −π), RGBA; the land drawn in its
	// colours, its relief kept as shading. None: the land as it is.
	struct Layer
	{
		u32 width, height;
		std::vector<u8> rgba;
	};
	void layer(std::optional<Layer> layer);
	// Whether the atlas's seams (where its regions meet) are drawn.
	void seams(bool shown) { m_seamsShown = shown; }
	// A body shown cut open (its index; none: none): its shells, from its card.
	void cutaway(std::optional<size_t> body) { m_system.cutaway(body); }

	// The same view as another's (a body swapped in for it): its camera, its
	// moment, the body it's round.
	void keep(const Planetarium &was);
	// Goes to another body of the system (its index there): the camera in
	// orbit round it, at its scale. The playable body's the one picked on.
	void focusBody(size_t body);
	size_t target() const { return m_target; }
	// the body it is round, or a flight is taking it to
	size_t heading() const { return m_flight ? m_flight->to : m_target; }
	const terragen::System &system() const { return m_system.system(); }
	void markers(std::vector<MarkerLayer::Marker> markers) { m_markers.set(std::move(markers)); }
	// The system's moment shown: days after its day 0. The playable body's
	// spin: its time of day (0..1, noon at ½ on longitude 0, as the game's
	// sun has it), or where its own sidereal day has turned it by then.
	void day(double days, std::optional<double> timeOfDay = std::nullopt);

	// Once a frame, drawn or not: the tiles for the camera on a screen this
	// size, built and taken onto the GPU. Throws if a tile couldn't be built.
	void update(v2u32 screen);
	// A frame into `view` (window pixels: the whole window, or the part a
	// page leaves for it); the scene already begun. Updates first.
	void draw(video::IVideoDriver *driver, core::recti view);

	const OrbitCamera &camera() const { return m_camera; }

private:
	const far::Palette m_palette; // before the tiles that read it
	const terragen::Atlas m_atlas;
	const double m_sea;
	far::Tiles m_tiles;
	OrbitCamera m_camera;
	SeamLayer m_seams;
	bool m_seamsShown = true;
	MarkerLayer m_markers;
	SystemLayer m_system;
	size_t m_target; // the body the camera's round (the playable one: its tiles')
	double m_day = 0;
	double m_spin = 0; // radians: the playable body turned, its ground to space
	v2f m_shift;       // where the view's middle is drawn (frame)
	std::shared_ptr<const Layer> m_layer; // the tint's image, shared with it
	// where the camera is from the playable body's centre (blocks), and how it looks
	OrbitCamera::Vec eye() const;
	core::matrix4 view() const;
	// a camera round a body, at its scale
	OrbitCamera cameraFor(size_t body) const;
	// a flight under way: from where the camera was (from its body's centre),
	// to a camera round another body
	struct Flying
	{
		size_t from;
		OrbitCamera::Vec start; // from `from`'s centre, blocks
		size_t to;
		OrbitCamera end;
		double seconds, t = 0;
	};
	std::optional<Flying> m_flight;
	struct Pose
	{
		OrbitCamera::Vec at, look; // the scene's (blocks)
	};
	Pose pose() const; // the camera now, flying or not
	OrbitCamera::Vec centreOf(size_t body) const; // in the scene
	std::chrono::steady_clock::time_point m_last; // the last update, for flights
	bool m_dragging = false;
	s32 m_x = 0, m_y = 0;          // the mouse, while dragging
	s32 m_downX = 0, m_downY = 0;  // where the press was
	std::optional<v2s32> m_click;  // a click not yet taken
	v2u32 m_screen{ 1, 1 };        // the last frame's view: its size,
	v2s32 m_corner{ 0, 0 };        // and where it is in the window
	u32 m_frame = 0;
	double m_worstFrame = 0, m_worstTiles = 0; // ms, since the last log
};

} // namespace planetarium
