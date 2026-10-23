// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "test.h"

#include "orbit/flight.h"
#include "terragen/climate.h"
#include <cmath>

using namespace orbit;

class TestOrbit : public TestBase {
public:
	TestOrbit() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestOrbit"; }

	void runTests(IGameDef *gamedef);

	void testPlanetCard();
	void testCircularOrbitCloses();
	void testSpinHelpsEastward();
	void testTerminalSpeed();
	void testCannonball();
	void testWhereRoundTrip();
};

static TestOrbit g_test_instance;

void TestOrbit::runTests(IGameDef *gamedef)
{
	TEST(testPlanetCard);
	TEST(testCircularOrbitCloses);
	TEST(testSpinHelpsEastward);
	TEST(testTerminalSpeed);
	TEST(testCannonball);
	TEST(testWhereRoundTrip);
}

static const Craft PERSON{ 100, 0.7 };

// at the equator, `altitude` up, moving east at `ground` m/s over the ground
static State east(const Flight &f, double altitude, double ground)
{
	return f.at({ 0, 0, altitude, { ground, 0, 0 } });
}

void TestOrbit::testPlanetCard()
{
	terragen::Planet p = terragen::home();
	// the weather and climate run on it at Earth's size: exactly Earth's day
	terragen::Planet e = terragen::resized(p, terragen::EARTH_RADIUS);
	UASSERT(e.day == 86400 && e.radius == terragen::EARTH_RADIUS && e.gravity == p.gravity);
	// its equator moves as Earth's does (~463 m/s)
	UASSERT(std::fabs(p.spin() * p.radius - 463.3) < 1);
	// low orbit is a few km/s
	double v = std::sqrt(p.mu() / (p.radius + 150e3));
	UASSERT(v > 2000 && v < 2500);
}

void TestOrbit::testCircularOrbitCloses()
{
	Flight f(terragen::home());
	const terragen::Planet &p = f.planet();
	double r = p.radius + 200e3, v = std::sqrt(p.mu() / r);
	// circular against the stars: over the ground, less the equator's motion there
	State s = east(f, 200e3, v - p.spin() * r);
	Elements e = f.elements(s);
	UASSERT(e.bound && std::fabs(e.apoapsis - 200e3) < 100 && std::fabs(e.periapsis - 200e3) < 100);
	UASSERT(e.inclination < 1e-6);
	// one period later it's back where it started against the stars; the
	// ground has turned under it
	double T = e.period;
	State later = f.step(s, PERSON, {}, T);
	double turned = p.spin() * T, ca = std::cos(turned), sa = std::sin(turned);
	Vec back{ ca * later.pos.x + sa * later.pos.z, later.pos.y, -sa * later.pos.x + ca * later.pos.z };
	UASSERT((back - s.pos).length() < 2000); // under 2 km after ~1/2 hour, with a whisper of air
	UASSERT(std::fabs(later.pos.length() - r) < 1000);
}

void TestOrbit::testSpinHelpsEastward()
{
	Flight f(terragen::home());
	const terragen::Planet &p = f.planet();
	double r = p.radius + 200e3, v = std::sqrt(p.mu() / r);
	State eastward = east(f, 200e3, v - p.spin() * r), westward = east(f, 200e3, -(v + p.spin() * r));
	UASSERT(std::fabs(f.elements(eastward).periapsis - f.elements(westward).periapsis) < 100);
	UASSERT(-westward.vel.x - eastward.vel.x > 2 * p.spin() * r - 1); // west costs ~930 m/s more
}

void TestOrbit::testTerminalSpeed()
{
	Flight f(terragen::home());
	// dropped near the ground: it settles at √(2mg / ρ·CdA)
	State s = f.near({ { 0, 3000, 0 }, {} }, PERSON, {}, 60);
	double expect = std::sqrt(2 * PERSON.mass * f.planet().gravity / (f.density(s.pos.y) * PERSON.drag_area));
	UASSERT(std::fabs(-s.vel.y - expect) < 0.02 * expect);
	// pushed sideways it keeps nothing of a curve: x moves straight
	State side = f.near({ { 0, 100, 0 }, { 50, 0, 0 } }, PERSON, {}, 1);
	UASSERT(side.vel.x < 50 && side.vel.x > 40 && side.pos.z == 0);
}

void TestOrbit::testCannonball()
{
	Flight f(terragen::home());
	const terragen::Planet &p = f.planet();
	// a cannonball from a mountain top comes down, not far
	Craft ball{ 10, 0.01 };
	Arc low = f.coast(east(f, 2000, 300), ball, 3600, 1);
	UASSERT(low.end == Arc::End::Ground && low.seconds < 120);
	// fast enough above the air, it goes all the way round
	double r = p.radius + 150e3;
	Arc high = f.coast(east(f, 150e3, std::sqrt(p.mu() / r) - p.spin() * r), ball, 7200, 5);
	UASSERT(high.end == Arc::End::Around);
	// the same speed down in the thick air never makes it
	Arc thick = f.coast(east(f, 2000, std::sqrt(p.mu() / r) - p.spin() * r), ball, 7200, 1);
	UASSERT(thick.end == Arc::End::Ground);
}

void TestOrbit::testWhereRoundTrip()
{
	Flight f(terragen::home());
	Where w{ 0.6, -2.1, 12345, { 10, -3, 7 } };
	Where b = f.where(f.at(w));
	UASSERT(std::fabs(b.latitude - w.latitude) < 1e-9 && std::fabs(b.longitude - w.longitude) < 1e-9);
	UASSERT(std::fabs(b.altitude - w.altitude) < 1e-6);
	UASSERT((b.velocity - w.velocity).length() < 1e-9);
}
