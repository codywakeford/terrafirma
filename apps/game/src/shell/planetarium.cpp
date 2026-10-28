// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "planetarium.h"
#include "bridge.h"
#include "shell.h"
#include <IVideoDriver.h>
#include <cmath>
#include <cstdio>

namespace shell {

using planetarium::MarkerLayer;
using View = planetarium::Planetarium;

static const char *const VERBS[] = { "planetarium.show", "planetarium.hide", "planetarium.focus", "planetarium.markers",
	"planetarium.viewport", "planetarium.fly", "planetarium.frame" };

// a finite number field, or none if it's absent; Refused if it's anything else
static std::optional<double> number(const Json::Value &v, const char *name)
{
	if (!v.isMember(name))
		return std::nullopt;
	const Json::Value &f = v[name];
	if (!f.isNumeric() || !std::isfinite(f.asDouble()))
		throw Refused(std::string("planetarium: '") + name + "' must be a number");
	return f.asDouble();
}

static double needed(const Json::Value &v, const char *name)
{
	const std::optional<double> n = number(v, name);
	if (!n)
		throw Refused(std::string("planetarium: needs '") + name + "'");
	return *n;
}

static std::vector<MarkerLayer::Marker> markersOf(const Json::Value &list)
{
	if (!list.isArray())
		throw Refused("planetarium: markers must be a list");
	std::vector<MarkerLayer::Marker> out;
	for (const Json::Value &m : list) {
		if (!m.isObject() || !m["colour"].isString())
			throw Refused("planetarium: a marker is { latitude, longitude, colour: \"#rrggbb\" }");
		unsigned r, g, b;
		const std::string colour = m["colour"].asString();
		if (colour.size() != 7 || std::sscanf(colour.c_str(), "#%2x%2x%2x", &r, &g, &b) != 3)
			throw Refused("planetarium: a marker's colour is \"#rrggbb\", not '" + colour + "'");
		out.push_back({ needed(m, "latitude"), needed(m, "longitude"), video::SColor(255, r, g, b) });
	}
	return out;
}

Planetarium::Planetarium(Opts opts) : m_opts(std::move(opts))
{
	m_opts.shell.on("planetarium.show", [this](const Json::Value &intent) { return show(intent); });
	m_opts.shell.on("planetarium.hide", [this](const Json::Value &) {
		hide();
		return Json::Value();
	});
	m_opts.shell.on("planetarium.focus", [this](const Json::Value &intent) { return focus(intent); });
	m_opts.shell.on("planetarium.markers", [this](const Json::Value &intent) { return markers(intent); });
	m_opts.shell.on("planetarium.viewport", [this](const Json::Value &intent) { return viewport(intent); });
	m_opts.shell.on("planetarium.fly", [this](const Json::Value &intent) { return fly(intent); });
	m_opts.shell.on("planetarium.frame", [this](const Json::Value &intent) { return frame(intent); });
}

Planetarium::~Planetarium()
{
	for (const char *verb : VERBS)
		m_opts.shell.off(verb);
}

void Planetarium::serve(Body body, std::optional<Json::Value> backdrop)
{
	m_backdrop.reset();
	hide();
	m_markers.clear();
	m_view.reset();
	m_insets = Insets{};
	m_body = std::move(body);
	m_backdrop = std::move(backdrop);
	if (m_backdrop)
		ask(*m_backdrop);
}

void Planetarium::ask(const Json::Value &body)
{
	if (!m_body)
		throw Refused("planetarium.show: no planet can be shown here now");
	if (shown() && body == m_asked)
		return; // that one's on screen (or coming): kept, not made again
	show(m_body(body));
	m_asked = body;
}

void Planetarium::show(View::Opts body)
{
	auto next = std::make_unique<View>(std::move(body));
	if (const View *was = m_current ? m_current.get() : m_next.get())
		next->keep(*was); // the same view of the new body
	next->markers(m_markers);
	m_next = std::move(next);
}

void Planetarium::day(double days)
{
	for (View *p : { m_current.get(), m_next.get() })
		if (p)
			p->day(days);
}

void Planetarium::dayIn(double worldDays, int32_t worldYear)
{
	for (View *p : { m_current.get(), m_next.get() })
		if (p)
			p->day(p->system().dayFor(worldDays, worldYear), worldDays - std::floor(worldDays));
}

void Planetarium::hide()
{
	if (m_backdrop) {
		// back to the universe behind the menu, over the whole window
		m_view.reset();
		m_insets = Insets{};
		m_markers.clear();
		ask(*m_backdrop);
		return;
	}
	m_current.reset();
	m_next.reset();
	m_asked = Json::Value();
}

Json::Value Planetarium::show(const Json::Value &intent)
{
	if (!intent.isObject())
		throw Refused("planetarium.show takes an object");
	if (intent.isMember("markers"))
		m_markers = markersOf(intent["markers"]);
	if (intent.isMember("viewport"))
		viewport(intent["viewport"]);
	ask(intent["body"]);
	if (intent.isMember("focus"))
		focus(intent["focus"]);
	return Json::Value();
}

Json::Value Planetarium::focus(const Json::Value &intent)
{
	if (!intent.isObject())
		throw Refused("planetarium.focus takes { latitude, longitude, altitude? } or { body }");
	if (intent.isMember("body")) {
		// another body of the system, by its id
		if (!intent["body"].isString())
			throw Refused("planetarium.focus: 'body' is a body's id");
		for (View *p : { m_current.get(), m_next.get() })
			if (p)
				p->focusBody(bodyIndex(*p, intent["body"]));
		return Json::Value();
	}
	const double lat = needed(intent, "latitude"), lon = needed(intent, "longitude");
	const std::optional<double> altitude = number(intent, "altitude");
	for (View *p : { m_current.get(), m_next.get() })
		if (p)
			p->focus(lat, lon, altitude);
	return Json::Value();
}

size_t Planetarium::bodyIndex(const View &p, const Json::Value &id) const
{
	if (!id.isString())
		throw Refused("planetarium: a body is named by its id");
	const auto &bodies = p.system().spec().bodies;
	for (size_t i = 0; i < bodies.size(); i++)
		if (bodies[i].id == id.asString())
			return i;
	throw Refused("planetarium: no body '" + id.asString() + "' in this system");
}

Json::Value Planetarium::fly(const Json::Value &intent)
{
	if (!intent.isObject())
		throw Refused("planetarium.fly takes { body?, latitude?, longitude?, altitude? or radii?, seconds }");
	const double seconds = needed(intent, "seconds");
	if (seconds < 0 || seconds > 60)
		throw Refused("planetarium.fly: a flight takes 0 to 60 seconds");
	const std::optional<double> lat = number(intent, "latitude"), lon = number(intent, "longitude"),
			altitude = number(intent, "altitude"), radii = number(intent, "radii");
	if (altitude && radii)
		throw Refused("planetarium.fly: an altitude or radii, not both");
	if (lat.has_value() != lon.has_value())
		throw Refused("planetarium.fly: a place is a latitude and a longitude");
	for (View *p : { m_current.get(), m_next.get() })
		if (p) {
			const size_t body = intent.isMember("body") ? bodyIndex(*p, intent["body"]) : p->target();
			p->fly({ body, lat, lon, altitude, radii, seconds });
		}
	return Json::Value();
}

Json::Value Planetarium::frame(const Json::Value &intent)
{
	if (!intent.isObject())
		throw Refused("planetarium.frame takes { left?, right?, top?, bottom? } (page pixels the page covers)");
	Insets in;
	for (auto [name, field] : { std::pair{ "left", &in.left }, { "right", &in.right }, { "top", &in.top }, { "bottom", &in.bottom } }) {
		const std::optional<double> v = number(intent, name);
		if (v && *v < 0)
			throw Refused(std::string("planetarium.frame: '") + name + "' can't be negative");
		*field = v.value_or(0); // an edge not given: the page covers none of it
	}
	m_insets = in;
	return Json::Value();
}

Json::Value Planetarium::markers(const Json::Value &intent)
{
	if (!intent.isObject())
		throw Refused("planetarium.markers takes { markers }");
	m_markers = markersOf(intent["markers"]);
	for (View *p : { m_current.get(), m_next.get() })
		if (p)
			p->markers(m_markers);
	return Json::Value();
}

Json::Value Planetarium::viewport(const Json::Value &intent)
{
	if (!intent.isObject())
		throw Refused("planetarium.viewport takes { x, y, width, height } (page pixels), or { whole: true }");
	if (intent.isMember("whole")) {
		if (!intent["whole"].isBool() || !intent["whole"].asBool())
			throw Refused("planetarium.viewport: 'whole' is true or absent");
		m_view.reset(); // the whole window
		return Json::Value();
	}
	const Rect r{ needed(intent, "x"), needed(intent, "y"), needed(intent, "width"), needed(intent, "height") };
	if (r.width <= 0 || r.height <= 0)
		throw Refused("planetarium.viewport: an empty rectangle");
	m_view = r;
	return Json::Value();
}

bool Planetarium::handle(const SEvent &event)
{
	View *p = m_current ? m_current.get() : m_next.get();
	return p && p->handle(event);
}

void Planetarium::draw(video::IVideoDriver *driver)
{
	const core::dimension2du screen = driver->getScreenSize();
	if (m_next) {
		// the old body stays until the new one is whole, if coarsely
		if (!m_current || (m_next->update(v2u32(screen.Width, screen.Height)), m_next->ready()))
			m_current = std::move(m_next);
	}
	if (!m_current)
		return;
	core::recti view(0, 0, screen.Width, screen.Height);
	if (m_view) {
		const float k = m_opts.shell.scale();
		view = core::recti((s32)std::lround(m_view->x * k), (s32)std::lround(m_view->y * k),
				(s32)std::lround((m_view->x + m_view->width) * k), (s32)std::lround((m_view->y + m_view->height) * k));
		view.clipAgainst(core::recti(0, 0, screen.Width, screen.Height));
	}
	// the planet framed in what the page leaves free: the view's middle moved there
	const float k = m_opts.shell.scale();
	const double w = std::max(1, view.getWidth()), h = std::max(1, view.getHeight());
	const double cx = (m_insets.left - m_insets.right) * k / 2, cy = (m_insets.top - m_insets.bottom) * k / 2;
	m_current->frame(v2f((f32)(cx / (w / 2)), (f32)(-cy / (h / 2))));
	m_current->draw(driver, view);
	if (const std::optional<View::Pick> pick = m_current->takeClick()) {
		Json::Value message;
		message["kind"] = "planetarium";
		message["event"] = "picked";
		message["latitude"] = pick->latitude;
		message["longitude"] = pick->longitude;
		Json::Value cell;
		static const char *const REGIONS[] = { "segment", "cap", "core" };
		cell["region"] = REGIONS[(int)pick->cell.region];
		cell["index"] = pick->cell.index;
		cell["i"] = (Json::Int64)pick->cell.i;
		cell["j"] = (Json::Int64)pick->cell.j;
		cell["k"] = (Json::Int64)pick->cell.k;
		message["cell"] = cell;
		m_opts.shell.send(message);
	}
}

} // namespace shell
