// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <include/cef_app.h>
#include <include/wrapper/cef_message_router.h>

namespace shell {

// The page's JS entry points: window.terraQuery / window.terraQueryCancel.
CefMessageRouterConfig routerConfig();

/*
	Process-level Chromium hooks, shared by the game process and the helpers
	it spawns: command-line switches, the terra:// scheme, and the renderer
	half of the message router.
*/
class App : public CefApp, public CefRenderProcessHandler
{
public:
	void OnBeforeCommandLineProcessing(const CefString &process_type,
			CefRefPtr<CefCommandLine> command_line) override;
	void OnRegisterCustomSchemes(CefRawPtr<CefSchemeRegistrar> registrar) override;
	CefRefPtr<CefRenderProcessHandler> GetRenderProcessHandler() override { return this; }

	void OnWebKitInitialized() override;
	void OnContextCreated(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
			CefRefPtr<CefV8Context> context) override;
	void OnContextReleased(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
			CefRefPtr<CefV8Context> context) override;
	bool OnProcessMessageReceived(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
			CefProcessId source_process, CefRefPtr<CefProcessMessage> message) override;

private:
	CefRefPtr<CefMessageRouterRendererSide> m_router;

	IMPLEMENT_REFCOUNTING(App);
};

// argc/argv as main() received them; CefInitialize needs them again later.
CefMainArgs mainArgs();

} // namespace shell
