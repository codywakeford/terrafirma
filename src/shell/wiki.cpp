// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "wiki.h"
#include "bridge.h"
#include "shell.h"

namespace shell {

static const char *const VERBS[] = {
	"matter.list", "matter.state", "matter.react", "sandbox.create", "sandbox.step", "sandbox.set", "sandbox.drop",
};

static const size_t MOST_SANDBOXES = 8; // a page shows one or two; more is a leak

Wiki::Wiki(Shell &shell) : m_shell(shell)
{
	m_shell.on("matter.list", [this](const Json::Value &) { return m_matter.list(); });
	m_shell.on("matter.state", [this](const Json::Value &intent) { return m_matter.state(intent); });
	m_shell.on("matter.react", [this](const Json::Value &intent) { return m_matter.react(intent); });
	// sandboxes are touched from the main thread and, stepping, from a worker
	// (a busy demo at speed takes tens of ms a step: never on the frame)
	m_shell.on("sandbox.create", [this](const Json::Value &intent) {
		std::lock_guard<std::mutex> lock(m_lock);
		return create(intent);
	});
	m_shell.onAsync("sandbox.step", [this](const Json::Value &intent) {
		std::lock_guard<std::mutex> lock(m_lock);
		return step(intent);
	});
	m_shell.on("sandbox.set", [this](const Json::Value &intent) {
		std::lock_guard<std::mutex> lock(m_lock);
		return set(intent);
	});
	m_shell.on("sandbox.drop", [this](const Json::Value &intent) {
		std::lock_guard<std::mutex> lock(m_lock);
		m_sandboxes.erase(intent["id"].asInt());
		return Json::Value();
	});
}

Wiki::~Wiki()
{
	for (const char *verb : VERBS)
		m_shell.off(verb);
}

static double number(const Json::Value &v, const char *field)
{
	if (!v[field].isNumeric())
		throw Refused(std::string("missing number '") + field + "'");
	return v[field].asDouble();
}

static sandbox::At at(const Json::Value &v)
{
	if (!v.isArray() || v.size() != 3 || !v[0].isInt() || !v[1].isInt() || !v[2].isInt())
		throw Refused("a block's position is [x, y, z]");
	return { v[0].asInt(), v[1].asInt(), v[2].asInt() };
}

static sandbox::Parts parts(const Json::Value &v)
{
	if (!v.isObject())
		throw Refused("parts are { substance: share }");
	sandbox::Parts out;
	for (const std::string &id : v.getMemberNames())
		out.push_back({ id, v[id].asDouble() });
	return out;
}

// One block's contents and settings, from the page.
static void apply(sandbox::Sandbox &s, const Json::Value &b)
{
	sandbox::At where = at(b["at"]);
	if (b.isMember("fill"))
		s.fill(where, parts(b["fill"]), number(b, "temperature"), b.isMember("gas") ? parts(b["gas"]) : sandbox::Parts{});
	else if (b.isMember("gas"))
		s.gas(where, parts(b["gas"]), number(b, "temperature"));
	else if (b.isMember("temperature"))
		s.touch(where, number(b, "temperature"));
	if (b.isMember("heater"))
		s.heater(where, number(b, "heater"));
}

Json::Value Wiki::create(const Json::Value &intent)
{
	if (m_sandboxes.size() >= MOST_SANDBOXES)
		throw Refused("too many sandboxes open: drop one first");
	const Json::Value &size = intent["size"];
	if (!size.isArray() || size.size() != 3)
		throw Refused("sandbox.create needs size: [x, y, z]");
	try {
		auto s = std::make_unique<sandbox::Sandbox>(sandbox::Opts{ m_matter.table(), size[0].asInt(), size[1].asInt(),
				size[2].asInt(), number(intent, "block"), number(intent, "ambient"), number(intent, "pressure"),
				intent.isMember("air") ? parts(intent["air"]) : sandbox::Parts{} });
		for (const Json::Value &b : intent["blocks"])
			apply(*s, b);
		int id = m_next++;
		m_sandboxes[id] = std::move(s);
		Json::Value out;
		out["id"] = id;
		return out;
	} catch (const std::runtime_error &e) { // SandboxError, MatterError
		throw Refused(e.what());
	}
}

sandbox::Sandbox &Wiki::find(const Json::Value &intent)
{
	auto it = m_sandboxes.find(intent["id"].asInt());
	if (it == m_sandboxes.end())
		throw Refused("no such sandbox (dropped, or never made)");
	return *it->second;
}

Json::Value Wiki::step(const Json::Value &intent)
{
	sandbox::Sandbox &s = find(intent);
	double seconds = number(intent, "seconds");
	if (seconds <= 0 || seconds > 3600)
		throw Refused("a sandbox steps 0..3600 s at a time");
	try {
		s.step(seconds);
	} catch (const std::runtime_error &e) {
		throw Refused(e.what());
	}
	return view(s);
}

Json::Value Wiki::set(const Json::Value &intent)
{
	sandbox::Sandbox &s = find(intent);
	try {
		apply(s, intent);
	} catch (const std::runtime_error &e) {
		throw Refused(e.what());
	}
	return view(s);
}

/*
	{ size, time, main[], state[], fill[], temperature[], activity[], vapour[],
	haze[] }: per block, x fastest, then z, then y from the bottom. main and
	vapour index matter.list's substances (−1: none); state as STATE_NAMES.
*/
Json::Value Wiki::view(const sandbox::Sandbox &s) const
{
	Json::Value out, size(Json::arrayValue);
	size.append(s.sizeX());
	size.append(s.sizeY());
	size.append(s.sizeZ());
	out["size"] = size;
	out["time"] = s.time();
	Json::Value main(Json::arrayValue), state(Json::arrayValue), fill(Json::arrayValue), t(Json::arrayValue),
			activity(Json::arrayValue), vapour(Json::arrayValue), haze(Json::arrayValue);
	for (const sandbox::BlockView &b : s.view()) {
		main.append(b.main);
		state.append(matter::STATE_NAMES[(int)b.state]);
		fill.append(b.fill);
		t.append(b.temperature);
		activity.append(b.activity);
		vapour.append(b.vapour);
		haze.append(b.haze);
	}
	out["main"] = main;
	out["state"] = state;
	out["fill"] = fill;
	out["temperature"] = t;
	out["activity"] = activity;
	out["vapour"] = vapour;
	out["haze"] = haze;
	return out;
}

} // namespace shell
