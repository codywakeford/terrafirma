// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <json/json.h>

namespace shell {

// Pure terragen preview shared by the create screen and the in-game map.
Json::Value previewJson(const Json::Value &intent);

} // namespace shell
