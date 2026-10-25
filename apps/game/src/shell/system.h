// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <json/json.h>

namespace shell {

/*
	The `system.preview` verb: a system's causes from the create screen in,
	every body's card and history out (terragen::bodies). The page holds
	only causes; everything it shows about a body comes from here. Throws
	Refused on causes the page shouldn't have been able to send.

	  { star, age, activity, bodies: [ { id, parent, orbit, eccentricity,
	    mass, formed, volatiles, day, life } ] }
*/
Json::Value systemPreview(const Json::Value &intent);

} // namespace shell
