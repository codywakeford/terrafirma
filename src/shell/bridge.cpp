// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "bridge.h"
#include "convert_json.h"
#include "log.h"
#include <json/json.h>
#include <sstream>

namespace shell {

enum QueryError { MALFORMED = 1, UNKNOWN_INTENT = 2 };

bool Bridge::OnQuery(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
		int64_t query_id, const CefString &request, bool persistent,
		CefRefPtr<Callback> callback)
{
	Json::Value intent;
	std::string errors;
	std::istringstream in(request.ToString());
	if (!Json::parseFromStream(Json::CharReaderBuilder(), in, &intent, &errors) ||
			!intent.isObject() || !intent["kind"].isString()) {
		errorstream << "shell: malformed intent: " << request.ToString() << std::endl;
		callback->Failure(MALFORMED, "malformed intent: " + errors);
		return true;
	}

	std::string kind = intent["kind"].asString();
	verbosestream << "shell: intent " << kind << std::endl;

	if (kind == "ready") {
		m_intents.ready();
	} else if (kind == "capture" && intent["keyboard"].isBool()) {
		m_intents.capture(intent["keyboard"].asBool());
	} else {
		errorstream << "shell: unknown intent: " << request.ToString() << std::endl;
		callback->Failure(UNKNOWN_INTENT, "unknown intent: " + kind);
		return true;
	}

	callback->Success("");
	return true;
}

void Bridge::send(CefRefPtr<CefBrowser> browser, const Json::Value &message)
{
	// JSON is a valid JS expression, so the message is passed as a literal.
	std::string js = "window.__terra.receive(" + fastWriteJson(message) + ")";
	browser->GetMainFrame()->ExecuteJavaScript(js, "terra://bridge", 0);
}

} // namespace shell
