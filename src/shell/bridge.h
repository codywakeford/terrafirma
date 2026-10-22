// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <include/wrapper/cef_message_router.h>
#include <functional>

namespace Json { class Value; }

namespace shell {

/*
	The engine's side of the page contract (ui/src/bridge/types.ts).
	Intents arrive as JSON queries; engine messages go out as JS calls.
*/
class Bridge : public CefMessageRouterBrowserSide::Handler
{
public:
	struct Intents {
		std::function<void()> ready;
		std::function<void(bool keyboard)> capture;
	};

	explicit Bridge(Intents intents) : m_intents(std::move(intents)) {}

	using CefMessageRouterBrowserSide::Handler::OnQuery;
	bool OnQuery(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
			int64_t query_id, const CefString &request, bool persistent,
			CefRefPtr<Callback> callback) override;

	void send(CefRefPtr<CefBrowser> browser, const Json::Value &message);

private:
	Intents m_intents;
};

} // namespace shell
