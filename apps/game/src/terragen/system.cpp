// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "system.h"
#include "code.h"
#include <cmath>
#include <stdexcept>

namespace terragen {

namespace {

constexpr double AU = 149597870.7;        // km
constexpr double EARTH_RADIUS = 6371.0;   // km
constexpr double SUN_RADIUS = 695700.0;   // km
constexpr double GM_SUN = 1.32712440018e11; // km³/s²
constexpr double GM_EARTH = 398600.4418;  // km³/s²
constexpr double DAY = 86400.0;           // s
constexpr double TAU = 6.283185307179586;

double real(fix v) { return v / (double)ONE; }

// a mass given as log₁₀ of Earth's, in hundredths (the page's slider)
fix earths(int hundredths) { return (fix)std::lround(std::pow(10.0, hundredths / 100.0) * ONE); }

SystemBody make(const char *id, const char *name, const char *colour, int32_t parent, double phase, double orbit, double e, int mass,
		double formed, double volatiles, double day, double tilt, bool life = false, bool playable = false)
{
	const auto f = [](double v) { return (fix)std::lround(v * ONE); };
	return { id, name, colour, { parent, f(orbit), f(e), earths(mass), f(formed), f(volatiles), f(day), life }, phase, 0, tilt,
		playable };
}

} // namespace

SystemSpec SystemSpec::home()
{
	SystemSpec s;
	s.star = { 2, (fix)std::lround(4.5 * ONE), HALF }; // yellow, 4.5 billion years, a middling wind
	s.bodies = {
		make("cinder", "Cinder", "#a76c55", -1, 0.18, 0.42, 0.2, -122, 0.1, 0.05, 1400, 1),
		make("earth", "Earth", "#50a4c5", -1, 0.62, 1, 0.017, 0, 0.35, 0.6, 24, 23, true, true),
		make("vesper", "Vesper", "#b47d5e", -1, 0.88, 1.6, 0.09, -90, 0.45, 0.5, 25, 25),
		make("jovian", "Jovian", "#d7ad83", -1, 0.35, 5.2, 0.05, 250, 0.8, 0.6, 10, 3),
		make("moon", "Moon", "#a8a8a1", 1, 0.1, 60, 0.055, -191, 0.35, 0.05, 655, 7),
		make("aster", "Aster", "#9a9188", 2, 0.4, 6, 0.02, -240, 0.5, 0.05, 30, 0),
		make("pyre", "Pyre", "#ce9464", 3, 0.2, 5.9, 0.0041, -182, 0.55, 0.05, 42, 0),
		make("nacre", "Nacre", "#c9d6da", 3, 0.55, 9.4, 0.009, -210, 0.8, 0.6, 85, 0),
		make("hale", "Hale", "#a9b9b4", 3, 0.75, 15, 0.0013, -160, 0.8, 0.5, 172, 0),
		make("morrow", "Morrow", "#868a91", 3, 0.05, 26, 0.007, -173, 0.85, 0.4, 400, 0),
	};
	s.bodies[4].inclination = 5; // the Moon's orbit, off the ecliptic as ours is
	return s;
}

SystemSpec SystemSpec::forWorld(const WorldCode &code)
{
	SystemSpec s = home();
	s.star.type = (int32_t)(code.star - STARS);
	for (SystemBody &b : s.bodies)
		if (b.playable)
			b.tilt = real(code.tilt);
	return s;
}

double System::dayFor(double worldDays, int32_t worldYear) const
{
	if (worldYear <= 0)
		throw std::invalid_argument("a world's year has days");
	return worldDays / worldYear * period(m_playable);
}

System::System(SystemSpec spec) : m_spec(std::move(spec))
{
	std::vector<BodyCauses> causes;
	size_t playable = 0;
	for (size_t i = 0; i < m_spec.bodies.size(); i++) {
		causes.push_back(m_spec.bodies[i].causes);
		if (m_spec.bodies[i].playable) {
			m_playable = i;
			playable++;
		}
	}
	if (playable != 1)
		throw std::invalid_argument("a system has one playable body, not " + std::to_string(playable));
	m_cards = bodies(m_spec.star, causes);
}

double System::radius(size_t body) const
{
	return real(m_cards.at(body).radius) * EARTH_RADIUS;
}

double System::luminosity() const
{
	const double m = real(STARS[m_spec.star.type].mass);
	return m * m * m * m; // as the body model takes it (code.h)
}

double System::starRadius() const
{
	return SUN_RADIUS * std::pow(real(STARS[m_spec.star.type].mass), 0.8);
}

double System::period(size_t body) const
{
	const BodyCauses &c = m_spec.bodies.at(body).causes;
	if (c.parent < 0) {
		const double a = real(c.orbit) * AU;
		return TAU * std::sqrt(a * a * a / (GM_SUN * real(STARS[m_spec.star.type].mass))) / DAY;
	}
	const double a = real(c.orbit) * radius((size_t)c.parent);
	const double gm = GM_EARTH * real(m_spec.bodies[(size_t)c.parent].causes.mass);
	return TAU * std::sqrt(a * a * a / gm) / DAY;
}

System::Vec System::local(size_t body, double days) const
{
	const SystemBody &b = m_spec.bodies.at(body);
	const double e = real(b.causes.eccentricity);
	const double a = b.causes.parent < 0 ? real(b.causes.orbit) * AU : real(b.causes.orbit) * radius((size_t)b.causes.parent);
	// the mean anomaly, then the eccentric (Kepler's equation, by Newton's method)
	double M = TAU * (b.phase + days / period(body));
	M -= TAU * std::floor(M / TAU);
	double E = e < 0.8 ? M : M_PI;
	for (int k = 0; k < 12; k++)
		E -= (E - e * std::sin(E) - M) / (1 - e * std::cos(E));
	const double x = a * (std::cos(E) - e), z = a * std::sqrt(1 - e * e) * std::sin(E);
	// tilted out of its parent's plane, about the line x
	const double i = b.inclination * M_PI / 180;
	return { x, z * std::sin(i), z * std::cos(i) };
}

System::Vec System::position(size_t body, double days) const
{
	Vec p = local(body, days);
	const int32_t parent = m_spec.bodies.at(body).causes.parent;
	if (parent >= 0) {
		const Vec q = local((size_t)parent, days);
		p = { p.x + q.x, p.y + q.y, p.z + q.z };
	}
	return p;
}

} // namespace terragen
