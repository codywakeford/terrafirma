// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "lab.h"
#include "bridge.h"
#include "exceptions.h"
#include "filesys.h"
#include <algorithm>
#include <fstream>

namespace shell {

Json::Value Lab::list() const
{
	std::vector<fs::DirListNode> mods = fs::GetDirListing(m_mods_dir);
	std::sort(mods.begin(), mods.end(), [](const auto &a, const auto &b) { return a.name < b.name; });

	Json::Value out(Json::arrayValue);
	for (const auto &mod : mods) {
		std::string path = m_mods_dir + DIR_DELIM + mod.name + DIR_DELIM "lab.json";
		if (!mod.dir || !fs::PathExists(path))
			continue;
		std::ifstream in(path);
		Json::Value file;
		std::string errors;
		if (!Json::parseFromStream(Json::CharReaderBuilder(), in, &file, &errors))
			throw BaseException("shell: " + path + " isn't JSON: " + errors);
		if (!file["scenarios"].isArray())
			throw BaseException("shell: " + path + " needs a \"scenarios\" array");
		for (Json::Value scenario : file["scenarios"]) {
			if (!scenario["id"].isString() || !scenario["title"].isString())
				throw BaseException("shell: a scenario in " + path + " has no id or title");
			scenario["mod"] = mod.name;
			out.append(scenario);
		}
	}
	return out;
}

Json::Value Lab::find(const std::string &id) const
{
	for (const Json::Value &scenario : list()) {
		if (scenario["id"].asString() == id)
			return scenario;
	}
	throw Refused("There's no Lab scenario called \"" + id + "\".");
}

} // namespace shell
