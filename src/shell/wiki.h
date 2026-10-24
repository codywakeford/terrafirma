// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "matter.h"
#include "orbit/flight.h"
#include "sandbox/sandbox.h"
#include <json/json.h>
#include <map>
#include <memory>
#include <mutex>

namespace shell {

class Shell;

/*
	The wiki's verbs, answered for the whole session (in the menu and in
	game alike): the matter tables (MatterVerbs) and the sandboxes its demos
	run in.

	  sandbox.create { size: [x, y, z], block (m), ambient (K), pressure (Pa),
	                   air: { id: share }, blocks: [{ at: [x, y, z], fill: { id: share },
	                   gas: { id: share } (with fill: the gas around it), temperature,
	                   heater (W) }] } → { id }
	  sandbox.step   { id, seconds } → the sandbox's view (see view()); off the main thread
	  sandbox.set    { id, at, temperature? heater? fill? gas? }
	  sandbox.drop   { id }
	  orbit.coast    { altitude, speed, angle, mass, drag_area, seconds }: Newton's cannonball
*/
class Wiki
{
public:
	explicit Wiki(Shell &shell);
	~Wiki();

private:
	Json::Value create(const Json::Value &intent);
	Json::Value step(const Json::Value &intent);
	Json::Value set(const Json::Value &intent);
	Json::Value coast(const Json::Value &intent);
	sandbox::Sandbox &find(const Json::Value &intent);
	Json::Value view(const sandbox::Sandbox &s) const;

	Shell &m_shell;
	MatterVerbs m_matter;
	orbit::Flight m_flight{ terragen::home() };
	std::mutex m_lock; // over m_sandboxes and each sandbox (stepping runs off the main thread)
	std::map<int, std::unique_ptr<sandbox::Sandbox>> m_sandboxes;
	int m_next = 1;
};

} // namespace shell
