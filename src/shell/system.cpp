// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "system.h"
#include "bridge.h"
#include "terragen/body.h"
#include <cstring>
#include <stdexcept>
#include <unordered_map>

namespace shell {

using namespace terragen;

static fix number(const Json::Value &v, const char *field, const std::string &where)
{
	if (!v[field].isNumeric())
		throw Refused(where + " needs a number '" + field + "'");
	double d = v[field].asDouble();
	if (d < -30000 || d > 30000)
		throw Refused(where + ": '" + field + "' is out of range");
	return (fix)(d * ONE);
}

static double real(fix v)
{
	return (double)v / ONE;
}

Json::Value systemPreview(const Json::Value &intent)
{
	StarCauses star{ -1, number(intent, "age", "the system"), number(intent, "activity", "the system") };
	for (int i = 0; i < 5; i++)
		if (intent["star"].isString() && intent["star"].asString() == STARS[i].id)
			star.type = i;
	if (star.type < 0)
		throw Refused("the system's star must be one of the star types");
	if (!intent["bodies"].isArray())
		throw Refused("the system needs a 'bodies' list");

	std::vector<BodyCauses> causes;
	std::vector<std::string> ids;
	std::unordered_map<std::string, int32_t> index;
	for (const Json::Value &b : intent["bodies"]) {
		if (!b["id"].isString())
			throw Refused("every body needs an id");
		std::string id = b["id"].asString(), where = "body " + id;
		int32_t parent = -1;
		if (!b["parent"].isNull()) {
			auto found = index.find(b["parent"].asString());
			if (found == index.end())
				throw Refused(where + ": its parent must come before it");
			parent = found->second;
		}
		if (!b["life"].isBool())
			throw Refused(where + " needs 'life' true or false");
		causes.push_back({ parent, number(b, "orbit", where), number(b, "eccentricity", where),
				number(b, "mass", where), number(b, "formed", where), number(b, "volatiles", where),
				number(b, "day", where), b["life"].asBool() });
		index[id] = (int32_t)ids.size();
		ids.push_back(id);
	}

	std::vector<BodyCard> cards;
	try {
		cards = bodies(star, causes);
	} catch (const std::invalid_argument &e) {
		throw Refused(e.what());
	}

	Json::Value out(Json::arrayValue);
	for (size_t i = 0; i < cards.size(); i++) {
		const BodyCard &c = cards[i];
		Json::Value j;
		j["id"] = ids[i];
		j["kind"] = KIND_NAMES[(int)c.kind];
		j["giant"] = c.giant;
		j["metal"] = real(c.metal);
		j["rock"] = real(c.rock);
		j["ice"] = real(c.ice);
		j["radius"] = real(c.radius);
		j["gravity"] = real(c.gravity);
		j["escape"] = real(c.escape);
		j["sunlight"] = real(c.sunlight);
		j["temperature"] = real(c.temperature);
		j["equilibrium"] = real(c.equilibrium);
		j["pressure"] = c.pressure < 0 ? Json::Value() : Json::Value(real(c.pressure));
		Json::Value gases(Json::objectValue);
		for (int g = 0; g < (int)Gas::Count; g++)
			if (c.gases[g] > 0)
				gases[GAS_NAMES[g]] = real(c.gases[g]);
		j["gases"] = gases;
		j["field"] = real(c.field);
		j["heat"] = real(c.heat);
		j["tidal"] = real(c.tidal);
		j["ocean"] = c.ocean;
		j["plates"] = c.plates;
		j["volcanism"] = c.volcanism;
		j["locked"] = c.locked;
		Json::Value layers(Json::arrayValue);
		for (const Layer &l : c.layers) {
			Json::Value e;
			e["name"] = l.name;
			e["outer"] = real(l.outer);
			e["molten"] = l.molten;
			layers.append(e);
		}
		j["layers"] = layers;
		Json::Value history(Json::arrayValue);
		for (const Moment &m : c.history) {
			Json::Value e;
			e["age"] = real(m.age);
			e["text"] = EVENT_TEXT[(int)m.event];
			history.append(e);
		}
		j["history"] = history;
		out.append(j);
	}
	return out;
}

} // namespace shell
