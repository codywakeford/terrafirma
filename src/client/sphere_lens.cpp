// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "sphere_lens.h"
#include "client/camera.h"
#include "client/client.h"
#include "constants.h"
#include "planet/planet.h"
#include <ICameraSceneNode.h>
#include <sstream>

namespace {

class SphereLens : public IShaderUniformSetter
{
public:
	explicit SphereLens(Client *client) : m_client(client) {}

	void onSetUniforms(video::IMaterialRendererServices *services) override
	{
		const planet::Planet *planet = m_client->getPlanet();
		if (!planet) {
			const float off[4] = {};
			m_eye.set(off, services);
			return;
		}
		Camera *camera = m_client->getCamera();
		const v3f at = camera->getPosition() / BS; // map nodes
		const int face = planet->nearest(at.X, at.Z);
		const terragen::Sphere::Lens<double> lens(planet->sphere(), planet->place(face, at.X, at.Z), at.Y);
		const float eye[4] = { float(lens.angleEast), float(lens.angleNorth), float(lens.eyeHeight), float(lens.radius) };
		const float step = float(lens.step);
		const v3s16 offset = camera->getOffset();
		const v3f scene = camera->getPosition() - intToFloat(offset, BS);
		const float net[4] = { float(planet::Planet::ORIGIN), float(planet->width() + planet::Planet::GAP),
			float(planet->half()), float(face) };
		m_eye.set(eye, services);
		m_step.set(&step, services);
		m_camera.set(scene, services);
		const auto row = [](const terragen::Sphere::Lens<double>::V3 &r) { return v3f(r.x, r.y, r.z); };
		m_east.set(row(lens.rows[0]), services);
		m_up.set(row(lens.rows[1]), services);
		m_north.set(row(lens.rows[2]), services);
		m_net.set(net, services);
		m_offset.set(v3f(offset.X, offset.Y, offset.Z), services);
		scene::ICameraSceneNode *node = camera->getCameraNode();
		core::matrix4 viewProj = node->getProjectionMatrix();
		viewProj *= node->getViewMatrix();
		m_view_proj.set(viewProj, services);
	}

private:
	Client *m_client;
	CachedVertexShaderSetting<float, 4> m_eye{"sphereEye"};
	CachedVertexShaderSetting<float> m_step{"sphereStep"};
	CachedVertexShaderSetting<float, 3> m_camera{"sphereCamera"};
	CachedVertexShaderSetting<float, 3> m_east{"sphereEast"};
	CachedVertexShaderSetting<float, 3> m_up{"sphereUp"};
	CachedVertexShaderSetting<float, 3> m_north{"sphereNorth"};
	CachedVertexShaderSetting<float, 4> m_net{"planetNet"};
	CachedVertexShaderSetting<float, 3> m_offset{"planetOffset"};
	CachedVertexShaderSetting<float, 16> m_view_proj{"sphereViewProj"};
};

} // namespace

IShaderUniformSetter *SphereLensFactory::create(const std::string &name)
{
	return new SphereLens(m_client);
}

std::string SphereLensFactory::prelude()
{
	std::ostringstream s;
	const auto vec = [&](const terragen::Sphere::Vec &v) {
		s << "vec3(" << v.x << ".0, " << v.y << ".0, " << v.z << ".0)";
	};
	s << "void planetAxes(highp float face, out highp vec3 o, out highp vec3 e, out highp vec3 n)\n{\n";
	for (int f = 0; f < 6; f++) {
		const terragen::Sphere::Axes &a = terragen::Sphere::axes(f);
		s << (f == 0 ? "\tif" : "\telse if") << " (face < " << f << ".5) { o = ";
		vec(a.out);
		s << "; e = ";
		vec(a.east);
		s << "; n = ";
		vec(a.north);
		s << "; }\n";
	}
	s << "}\n";
	return s.str();
}
