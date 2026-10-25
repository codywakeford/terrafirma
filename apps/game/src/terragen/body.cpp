// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "body.h"
#include <algorithm>
#include <optional>
#include <stdexcept>
#include <string>

namespace terragen {

const char *const KIND_NAMES[(int)Kind::Count] = {
	"temperate", "cold desert", "hothouse", "volcanic", "ice shell", "hydrocarbon", "airless", "giant",
};

const char *const GAS_NAMES[(int)Gas::Count] = { "CO2", "N2", "O2", "CH4", "H2", "He" };

const char *const EVENT_TEXT[(int)Event::Count] = {
	"a magnetic field switches on",
	"the core freezes; the magnetic field fades",
	"oceans condense",
	"the oceans boil away",
	"the oceans freeze",
	"brief seas come and go",
	"plate tectonics begins",
	"the plates lock still",
	"the volcanoes fall silent",
	"the air is lost to space",
	"a runaway greenhouse",
	"life appears in the oceans",
	"oxygen builds up in the air",
};

static fix F(int32_t hundredths) { return ratio(hundredths, 100); }

static fix minx(fix a, fix b) { return a < b ? a : b; }

// a^b that is 0 for a ≤ 0
static fix power(fix a, fix b)
{
	return a <= 0 ? 0 : pow(a, b);
}

/*
	A history is told as changes that last: a state must hold for HOLD before
	it becomes part of the story (dated from when it began). One that keeps
	flipping back and forth is told once, if it has a way to say so.
*/
class Story
{
public:
	struct Thread
	{
		std::optional<Event> on, off; // what each change is called; none isn't news
		std::optional<Event> flickers;
		bool known = false, state = false;
		bool waiting = false, next = false;
		fix since = 0;
		int flips = 0;
		bool told_flicker = false;
	};

	explicit Story(std::vector<Moment> &out) : m_out(out) {}

	void mark(Thread &th, fix t, bool value)
	{
		if (th.known && th.state == value) {
			th.waiting = false;
			return;
		}
		if (!th.waiting || th.next != value) {
			th.waiting = true;
			th.next = value;
			th.since = t;
			if (++th.flips >= 6 && th.flickers && !th.told_flicker) {
				th.told_flicker = true;
				m_out.push_back({ t, *th.flickers });
			}
			return;
		}
		if (t - th.since < HOLD)
			return;
		th.known = true;
		th.state = value;
		th.waiting = false;
		// a starting state that's "off" isn't news; only an ending is
		std::optional<Event> said = value ? th.on : th.off;
		if (said && (value || th.since > F(20)))
			m_out.push_back({ th.since, *said });
	}

private:
	static constexpr fix HOLD = 13107; // 0.2 Gyr
	std::vector<Moment> &m_out;
};

static void check(const BodyCauses &c, size_t i, const std::vector<BodyCauses> &all)
{
	auto fail = [&](const std::string &why) {
		throw std::invalid_argument("body " + std::to_string(i) + ": " + why);
	};
	if (c.parent >= (int32_t)i)
		fail("its parent must be listed before it");
	if (c.parent >= 0 && all[c.parent].parent >= 0)
		fail("a moon can't have moons");
	if (c.mass < F(1) / 4 || c.mass > toFix(400))
		fail("mass must be 0.0025..400 Earth masses");
	if (c.parent < 0 ? c.orbit < F(5) || c.orbit > toFix(100) : c.orbit < F(150) || c.orbit > toFix(200))
		fail("orbit must be 0.05..100 AU around the star, 1.5..200 radii around a planet");
	if (c.eccentricity < 0 || c.eccentricity > F(90))
		fail("eccentricity must be 0..0.9");
	if (c.formed < 0 || c.formed > ONE || c.volatiles < 0 || c.volatiles > ONE)
		fail("formed and volatiles are 0..1");
	if (c.day < ONE)
		fail("a day is at least an hour");
}

std::vector<BodyCard> bodies(const StarCauses &star, const std::vector<BodyCauses> &causes)
{
	if (star.type < 0 || star.type >= 5)
		throw std::invalid_argument("no star type " + std::to_string(star.type));
	if (star.age < F(10) || star.age > toFix(13))
		throw std::invalid_argument("a system's age is 0.1..13 billion years");

	const fix luminosity = power(STARS[star.type].mass, toFix(4));
	std::vector<BodyCard> cards;
	cards.reserve(causes.size());

	for (size_t i = 0; i < causes.size(); i++) {
		const BodyCauses &c = causes[i];
		check(c, i, causes);
		const BodyCauses *parent = c.parent >= 0 ? &causes[c.parent] : nullptr;
		const BodyCard *parent_card = c.parent >= 0 ? &cards[c.parent] : nullptr;
		BodyCard b{};

		// what it's made of, and so its size
		b.giant = (c.mass >= toFix(10) && c.formed >= HALF) || c.mass >= toFix(50);
		if (b.giant) {
			b.metal = F(5);
			b.ice = F(10);
			b.rock = F(15);
			b.radius = F(350) + mul(toFix(8), ONE - exp(-c.mass / 90));
		} else {
			b.ice = clamp(mul(c.formed - HALF, F(160)), 0, F(70));
			b.metal = mul(ONE - b.ice, F(40) - mul(F(20), c.formed));
			b.rock = ONE - b.ice - b.metal;
			b.radius = mul(power(c.mass, F(27)), ONE + mul(F(45), b.ice) - mul(F(35), b.metal - F(32)));
		}
		b.gravity = div(c.mass, mul(b.radius, b.radius));
		b.escape = mul(ratio(112, 10), sqrt(div(c.mass, b.radius)));

		// where it is: starlight, and tides from a parent
		fix distance = parent ? parent->orbit : c.orbit; // AU from the star
		b.sunlight = div(luminosity, mul(distance, distance));
		b.equilibrium = mul(mul(toFix(278), power(b.sunlight, F(25))), ratio(915, 1000));
		if (parent) {
			fix l = 2 * log2(parent->mass) + 5 * log2(b.radius) + log2(c.eccentricity + ratio(1, 1000))
					- 6 * log2(c.orbit) + ratio(197, 10);
			b.tidal = minx(exp2(l), toFix(1000)); // beyond this a moon is molten anyway
		}
		b.locked = parent ? c.orbit <= toFix(80) : distance < mul(F(12), sqrt(STARS[star.type].mass));
		const fix spin = sqrt(minx(ONE, div(toFix(24), c.day)));
		const fix parent_field = parent_card && parent_card->giant ? parent_card->field : 0;

		// the air and water it formed with
		const bool inner = c.formed < F(70); // inside the frost line: CO₂-rich; beyond: nitrogen and methane
		fix co2 = 0, n2 = 0, o2 = 0, ch4 = 0, water = 0;
		{
			fix first = mul(c.volatiles, toFix(2) + 30 * b.ice);
			if (inner) {
				co2 = 7 * first;
				n2 = mul(F(16), first);
			} else {
				n2 = mul(F(75), first);
				ch4 = mul(F(25), first);
			}
			water = mul(F(150), first);
		}

		auto interior = [&](fix t) {
			fix past = ratio(45, 10) - t;
			return mul(power(c.mass, F(60)),
					mul(F(80), exp2(div(past, F(250)))) + mul(F(20), exp2(div(past, F(120)))));
		};
		auto climate = [&](fix air, fix co2_now, fix &pressure, fix &temperature) {
			pressure = mul(air, b.gravity);
			fix greenhouse = 0;
			if (!b.giant && pressure > 0)
				greenhouse = mul(mul(toFix(33), power(pressure, F(35))), ONE + 2 * div(co2_now, air));
			temperature = b.equilibrium + greenhouse;
		};

		Story story(b.history);
		Story::Thread field_th{ Event::FieldOn, Event::FieldFades, std::nullopt };
		Story::Thread ocean_th{ Event::OceansForm, Event::OceansFreeze, Event::SeasComeAndGo };
		Story::Thread plates_th{ Event::PlatesBegin, Event::PlatesStop, std::nullopt };
		Story::Thread volcano_th{ std::nullopt, Event::VolcanoesSilent, std::nullopt };
		Story::Thread air_th{ std::nullopt, Event::AirLost, std::nullopt };
		bool life_told = false, oxygen_told = false, runaway_told = false;

		const fix dt = ratio(5, 100);
		fix t = 0, pressure = 0, temperature = b.equilibrium, heat = 0;
		bool ocean = false, plates = false;
		while (t < star.age) {
			t = minx(star.age, t + dt);
			heat = interior(t) + b.tidal;

			// volcanoes add air; a warm interior is needed
			fix out = mul(mul(mul(c.volatiles, std::max<fix>(0, minx(heat, toFix(6)) - F(25))), dt), F(90));
			if (inner) {
				co2 += 7 * out;
				n2 += mul(F(16), out);
			} else {
				n2 += 6 * out;
				ch4 += 2 * out;
			}
			water += mul(F(150), out);
			fix air = co2 + n2 + o2 + ch4;
			climate(air, co2, pressure, temperature);

			// the core: cools by its own clock, small ones fast; spinning makes a field
			fix core_heat = mul(12 * c.mass, exp2(-div(t, F(120))));
			bool core = !b.giant && b.metal > F(10) && core_heat > F(35);
			b.field = b.giant ? minx(toFix(20), c.mass / 30)
					: core ? mul(mul(div(b.metal, F(32)), spin), minx(ONE, core_heat)) : 0;

			fix boiling = toFix(373) + 30 * log2(pressure + ONE);
			ocean = b.sunlight < F(130) && temperature > toFix(273) && temperature < boiling &&
					pressure > ratio(6, 1000) && water > F(5);

			// losses: thermal escape (light, hot worlds), the stellar wind where no
			// field shields, and a giant's magnetosphere for its close moons
			// Jeans: half the escape parameter, Earth ≈ 80 (held for good)
			fix hold = minx(mul(toFix(80), power(div(b.escape, ratio(112, 10)), toFix(2))), toFix(100));
			hold = minx(mul(hold, div(toFix(300), std::max<fix>(temperature, toFix(30)))), toFix(100));
			fix thermal = 50 * exp(-hold);
			fix young = ONE + 4 * exp2(-div(t, F(70)));
			// closer than 0.1 AU it strips everything regardless
			fix near = std::max<fix>(mul(distance, distance), F(1));
			fix wind = minx(div(mul(mul(mul(20 * star.activity, young), std::max<fix>(0, ONE - b.field)),
					exp(-power(div(b.escape, toFix(5)), toFix(2)))), near), toFix(1000));
			fix sputter = parent ? mul(3 * parent_field, power(div(toFix(5), c.orbit), toFix(3))) : 0;
			fix loss = minx(F(90), mul(thermal + wind + sputter, dt));
			co2 = mul(co2, ONE - loss);
			n2 = mul(n2, ONE - loss);
			o2 = mul(o2, ONE - loss);
			ch4 = mul(ch4, ONE - loss);
			bool steam = temperature > toFix(330) && !ocean && water > F(5);
			if (steam) {
				water = mul(water, ONE - minx(F(90), mul(F(60), dt) + loss)); // its hydrogen escapes
				// a runaway boils an ocean's worth of water under air that traps the
				// heat (Venus); a world that formed dry (Mercury) just dries further
				if (!runaway_told && b.sunlight >= F(130) && pressure > F(10) && water > HALF) {
					runaway_told = true;
					b.history.push_back({ t, Event::Runaway });
				}
			}
			water = mul(water, ONE - loss);

			plates = !b.giant && ocean && heat > F(40) && heat < toFix(5) && c.mass >= F(30);
			if (ocean) // carbon into rock where plates recycle it, some into the sea otherwise
				co2 = mul(co2, plates ? F(2) : F(70));
			if (c.life && ocean && t > F(80)) {
				if (!life_told) {
					life_told = true;
					b.history.push_back({ t, Event::LifeAppears });
				}
				if (t > F(240))
					o2 = minx(mul(n2, F(27)), o2 + mul(F(10), dt));
			}
			if (o2 > F(5) && !oxygen_told) {
				oxygen_told = true;
				b.history.push_back({ t, Event::OxygenRises });
			}

			if (!b.giant) {
				story.mark(field_th, t, b.field > F(10));
				story.mark(ocean_th, t, ocean);
				story.mark(plates_th, t, plates);
				story.mark(volcano_th, t, heat > F(30));
				story.mark(air_th, t, pressure > ratio(3, 1000));
			}
			climate(co2 + n2 + o2 + ch4, co2, pressure, temperature);
		}
		std::stable_sort(b.history.begin(), b.history.end(),
				[](const Moment &a, const Moment &z) { return a.age < z.age; });

		b.heat = heat;
		b.ocean = ocean;
		b.plates = plates;
		b.volcanism = heat > F(30);
		b.temperature = temperature;
		fix air = co2 + n2 + o2 + ch4;
		if (b.giant) {
			b.pressure = -ONE;
			b.gases[(int)Gas::H2] = F(86);
			b.gases[(int)Gas::He] = F(14);
		} else {
			b.pressure = pressure;
			if (pressure >= ratio(1, 1000) && air > 0) {
				b.gases[(int)Gas::CO2] = div(co2, air);
				b.gases[(int)Gas::N2] = div(n2, air);
				b.gases[(int)Gas::O2] = div(o2, air);
				b.gases[(int)Gas::CH4] = div(ch4, air);
			}
		}

		// the inside: a core sized by its metal (iron is about twice as dense
		// as the whole), a mantle, a thin crust, then water or ice
		if (b.giant) {
			b.layers = { { "core", F(12), false }, { "metallic hydrogen", F(70), true }, { "envelope", ONE, true } };
		} else {
			fix core = power(mul(b.metal, HALF), ratio(1, 3));
			bool liquid_core = b.field > F(10) || heat > toFix(3);
			// ice is lighter than rock: its share of the volume is half again its share of the mass
			fix ice_volume = minx(mul(b.ice, F(150)), F(80));
			fix ice = b.ice > F(5) ? ONE - power(ONE - ice_volume, ratio(1, 3)) : 0;
			b.layers.push_back({ "core", std::max<fix>(core, F(5)), liquid_core });
			b.layers.push_back({ "mantle", ONE - ice - F(2), false });
			// a surface ocean sits on the crust: a hundredth of the radius, so it shows
			b.layers.push_back({ "crust", ONE - ice - (ice == 0 && b.ocean ? F(1) : 0), false });
			if (ice > 0) {
				// a tidally warmed ice world hides an ocean under its shell
				if (b.tidal > F(30))
					b.layers.push_back({ "ocean", ONE - ice / 3, true });
				b.layers.push_back({ "ice", ONE, false });
			} else if (b.ocean) {
				b.layers.push_back({ "ocean", ONE, true });
			}
		}

		if (b.giant)
			b.kind = Kind::Giant;
		else if (heat > toFix(8))
			b.kind = Kind::Volcanic;
		else if (b.ice > F(30) && b.tidal > F(30) && temperature < toFix(200))
			b.kind = Kind::IceShell;
		else if (pressure < ratio(1, 1000))
			b.kind = Kind::Airless;
		else if (temperature > toFix(400))
			b.kind = Kind::Hothouse;
		else if (ocean)
			b.kind = Kind::Temperate;
		else if (temperature < toFix(140) && pressure > F(30) && ch4 > 0)
			b.kind = Kind::Hydrocarbon;
		else
			b.kind = Kind::ColdDesert;
		cards.push_back(std::move(b));
	}
	return cards;
}

} // namespace terragen
