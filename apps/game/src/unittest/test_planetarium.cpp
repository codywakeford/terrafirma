// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "test.h"

#include "constants.h"
#include "planetarium/camera.h"
#include <algorithm>
#include <cmath>

using planetarium::OrbitCamera;

/*
	The Planetarium's camera (planetarium/camera.h): in orbit round a body,
	looking at its centre, its tiles' eye under it.
*/
class TestPlanetarium : public TestBase {
public:
	TestPlanetarium() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestPlanetarium"; }

	void runTests(IGameDef *gamedef);

	void testZoomStaysOverTheBody();
	void testDragSlowsNearTheGround();
	void testLooksAtTheCentre();
	void testRayThroughTheScreen();
	void testSpinTurnsTheBody();
	void testDragOverThePole();
	void testCentreIsAhead();
};

static TestPlanetarium g_test_instance;

void TestPlanetarium::runTests(IGameDef *gamedef)
{
	TEST(testZoomStaysOverTheBody);
	TEST(testDragSlowsNearTheGround);
	TEST(testLooksAtTheCentre);
	TEST(testRayThroughTheScreen);
	TEST(testSpinTurnsTheBody);
	TEST(testDragOverThePole);
	TEST(testCentreIsAhead);
}

static constexpr double SEA = 1305;

static double length(const OrbitCamera::Vec &v)
{
	return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

// However far it's zoomed, it stays between 100 blocks over the sea and a
// few radii out; the tiles' eye is at the sea under it.
void TestPlanetarium::testZoomStaysOverTheBody()
{
	OrbitCamera c(SEA, 6 * SEA);
	c.zoom(1000);
	UASSERT(std::fabs(c.altitude() - 100) < 1e-9);
	c.zoom(-1000);
	UASSERT(c.altitude() <= 6 * SEA + 1e-9 && c.altitude() > 5 * SEA);
	const far::Eye eye = c.eye(1080);
	UASSERT(std::fabs(length(eye.at) - SEA) < 1e-6);
	const OrbitCamera::Vec p = c.position();
	const double d = length(p);
	UASSERT(std::fabs(d - SEA - c.altitude()) < 1e-6);
	UASSERT(std::fabs(eye.at.x / SEA - p.x / d) < 1e-9 && std::fabs(eye.at.y / SEA - p.y / d) < 1e-9);
	UASSERT(eye.range > 0 && eye.range <= 2 * SEA + 1e-3 && eye.cut == 0);
	// it reaches its horizon: the chord to where its sight line grazes the sea
	const double horizon = std::acos(SEA / (SEA + c.altitude()));
	UASSERT(eye.range >= 2 * SEA * std::sin(horizon / 2));
	// close up it sees less of the body, and tiles split further off (in their spans)
	c.zoom(1000);
	const far::Eye low = c.eye(1080);
	UASSERT(low.range < eye.range / 3);
	UASSERT(std::fabs(low.split - eye.split) < 1e-6 && c.eye(2160).split > 1.9 * low.split);
}

// A pixel's drag moves the camera round less the lower it is.
void TestPlanetarium::testDragSlowsNearTheGround()
{
	const auto moved = [](double steps) {
		OrbitCamera c(SEA, 6 * SEA);
		c.zoom(steps);
		const OrbitCamera::Vec a = c.position();
		c.drag(10, 0);
		const OrbitCamera::Vec b = c.position();
		const double angle = std::acos(std::clamp((a.x * b.x + a.y * b.y + a.z * b.z) / (length(a) * length(b)), -1.0, 1.0));
		return angle;
	};
	UASSERT(moved(30) < moved(0) / 5);
}

// The body's centre is drawn in the middle of the screen, in front of the camera.
void TestPlanetarium::testLooksAtTheCentre()
{
	OrbitCamera c(SEA, 6 * SEA);
	c.drag(123, -45);
	const OrbitCamera::Vec p = c.position();
	core::matrix4 m = c.projection(16.0f / 9);
	m *= c.view();
	const v3f centre = v3f((float)-p.x, (float)-p.y, (float)-p.z) * BS; // the centre, from the camera
	f32 clip[4] = { centre.X, centre.Y, centre.Z, 1 };
	m.multiplyWith1x4Matrix(clip);
	UASSERT(clip[3] > 0);
	UASSERT(std::fabs(clip[0] / clip[3]) < 1e-3 && std::fabs(clip[1] / clip[3]) < 1e-3);
	UASSERT(clip[2] / clip[3] > -1 && clip[2] / clip[3] < 1);
}

// Through the screen's middle the ray is straight at the centre; right of it,
// it leans east (the screen's right, with north up); focus puts the camera
// over the place asked.
void TestPlanetarium::testRayThroughTheScreen()
{
	OrbitCamera c(SEA, 6 * SEA);
	c.focus(0.3, 1.1, 2000.0);
	UASSERT(std::fabs(c.altitude() - 2000) < 1e-9);
	const OrbitCamera::Vec p = c.position();
	const double d = length(p);
	UASSERT(std::fabs(std::asin(p.y / d) - 0.3) < 1e-9 && std::fabs(std::atan2(p.z, p.x) - 1.1) < 1e-9);
	const OrbitCamera::Vec mid = c.ray(960, 540, 1920, 1080);
	UASSERT(std::fabs((mid.x * p.x + mid.y * p.y + mid.z * p.z) / d + 1) < 1e-5); // −up
	const OrbitCamera::Vec right = c.ray(1500, 540, 1920, 1080);
	const OrbitCamera::Vec east{ -std::sin(1.1), 0, std::cos(1.1) };
	UASSERT(right.x * east.x + right.y * east.y + right.z * east.z > 0.1);
	const OrbitCamera::Vec top = c.ray(960, 100, 1920, 1080);
	const OrbitCamera::Vec north{ -std::sin(0.3) * std::cos(1.1), std::cos(0.3), -std::sin(0.3) * std::sin(1.1) };
	UASSERT(top.x * north.x + top.y * north.y + top.z * north.z > 0.1);
}

// A spun body: its ground's longitude grows by the spin, and the view that
// draws it turned puts its points where turn() says.
void TestPlanetarium::testSpinTurnsTheBody()
{
	const OrbitCamera::Vec east = OrbitCamera::turn({ 1, 0, 0 }, M_PI / 2);
	UASSERT(std::fabs(east.z - 1) < 1e-12 && std::fabs(east.x) < 1e-12);
	OrbitCamera c(SEA, 6 * SEA);
	c.focus(0.2, -0.7, 3000.0);
	const core::matrix4 view = c.view(), spun = OrbitCamera::turned(view, 0.9);
	const OrbitCamera::Vec p{ 300, -120, 800 }, q = OrbitCamera::turn(p, 0.9);
	v3f a((f32)p.x, (f32)p.y, (f32)p.z), b((f32)q.x, (f32)q.y, (f32)q.z);
	spun.transformVect(a);
	view.transformVect(b);
	UASSERT((a - b).getLength() < 1e-2f);
}

// Dragged north, it goes over the pole and down the other side, still
// looking at the centre; no latitude holds it back.
void TestPlanetarium::testDragOverThePole()
{
	OrbitCamera c(SEA, 6 * SEA);
	c.focus(1.2, 0.5);
	const double k = 0.004; // TURN at this height (a radius or more up)
	c.drag(0, (M_PI / 2 - 1.2 + 0.4) / k); // past the pole by 0.4 rad
	UASSERT(std::fabs(c.latitude() - (M_PI / 2 - 0.4)) < 1e-6);
	UASSERT(std::fabs(std::remainder(c.longitude() - (0.5 + M_PI), 2 * M_PI)) < 1e-6);
	const OrbitCamera::Vec p = c.position();
	UASSERT(std::fabs(length(p) - (SEA + c.altitude())) < 1e-6);
}

// The view takes a point from the camera into its own axes (as the hover
// test projects bodies): the body's centre straight ahead, its north up.
void TestPlanetarium::testCentreIsAhead()
{
	OrbitCamera c(SEA, 6 * SEA);
	c.focus(0.3, 1.1);
	const OrbitCamera::Vec p = c.position();
	core::vector3df centre((f32)-p.x, (f32)-p.y, (f32)-p.z);
	c.view().transformVect(centre);
	UASSERT(std::fabs(centre.X) < 1e-2 && std::fabs(centre.Y) < 1e-2);
	UASSERT(std::fabs(centre.Z - length(p)) < 1e-2 * length(p));
	// a point north of the centre is above it on the screen
	core::vector3df north((f32)-p.x, (f32)(-p.y + SEA), (f32)-p.z);
	c.view().transformVect(north);
	UASSERT(north.Y > 0);
}
