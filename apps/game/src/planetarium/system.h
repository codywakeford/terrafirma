// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "irrlichttypes_bloated.h"
#include "irr_ptr.h"
#include "terragen/system.h"
#include <CMeshBuffer.h>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace video { class IVideoDriver; class ITexture; }

/*
	The rest of a body's system, round it (planning/sprint-35-system.md): the
	star, the other planets and moons, their orbits. In the focused body's
	units (blocks, its centre the origin; the system's km × blocksPerKm),
	each drawn relative to the camera in doubles. Every body is drawn at the
	detail its distance calls for: a lit sphere coloured by its kind, or a
	dot where it would be under a couple of pixels; the star a bright sphere
	and a glow, the light all the spheres are lit by. Far to near, each body
	with its own depth range, so the far side of the system and the ground
	under the camera share one frame. The focused body is drawn by whoever
	owns it (the Planetarium: its tiles), in its turn.
*/
namespace planetarium {

class SystemLayer
{
public:
	struct Opts
	{
		std::shared_ptr<const terragen::System> system;
		size_t focus;       // the body at the origin, drawn by the caller
		double blocksPerKm; // the focused body's scale
	};
	// (The scene is the focused body's frame: its centre the origin, its y
	// its spin axis, tilted from its orbit's by its tilt.)
	explicit SystemLayer(Opts opts);
	~SystemLayer();

	struct Camera
	{
		terragen::System::Vec at; // from the focused body's centre, blocks
		core::matrix4 view;       // camera-relative (at the origin)
		float fov, aspect;        // radians (vertical), width / height
		v2f shift;                // where the view's middle is drawn (OrbitCamera::perspective)
		u32 height;               // pixels
	};
	// The system `day` days after its day 0, the scene begun. `focused`
	// draws the focused body, given the depth range it may use (BS units).
	void draw(video::IVideoDriver *driver, const Camera &camera, double day,
			const std::function<void(float near, float far)> &focused);

	// The farthest any body is from the focused one (blocks): how far out a
	// camera may want to go to see it all.
	double extent() const { return m_extent; }
	// Where a body is in the scene `day` days on (blocks), and how big (blocks).
	terragen::System::Vec centre(size_t body, double day) const;
	terragen::System::Vec starCentre(double day) const;
	double radius(size_t body) const { return m_opts.system->radius(body) * m_opts.blocksPerKm; }
	const terragen::System &system() const { return *m_opts.system; }
	// A body drawn cut open (none: none): its shells from its card (core,
	// mantle, crust, ocean, …) as spheres, a window cut in them toward the
	// camera. The focused body too: then it's drawn here, not by its owner.
	void cutaway(std::optional<size_t> body) { m_cutaway = body; }

private:
	struct Body
	{
		terragen::System::Vec at; // from the camera, blocks
		double radius;            // blocks
		double distance;          // from the camera, blocks
		int index;                // the system's body; −1 the star
	};
	void sphere(video::IVideoDriver *driver, const Body &b, video::SColor colour, const terragen::System::Vec &sun, bool lit);
	void glow(video::IVideoDriver *driver, const Camera &camera, const Body &star);
	void shells(video::IVideoDriver *driver, const Body &b, const terragen::System::Vec &sun);
	void orbits(video::IVideoDriver *driver, const Camera &camera, double day);

	// a vector of the system (km, the ecliptic's axes) in the scene's (blocks)
	terragen::System::Vec scene(const terragen::System::Vec &km) const;

	const Opts m_opts;
	double m_tilt; // radians: the focused body's
	double m_extent = 0;
	std::optional<size_t> m_cutaway;
	std::vector<u16> m_window; // a cut sphere's triangles, this frame
	irr_ptr<scene::SMeshBuffer> m_sphere; // unit, its normals its points
	video::ITexture *m_glow = nullptr;
	video::IVideoDriver *m_driver = nullptr; // the glow's, to take it off
};

} // namespace planetarium
