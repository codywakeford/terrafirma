// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "planetarium/planetarium.h"
#include <json/json.h>
#include <functional>
#include <memory>

namespace shell {

class Shell;

/*
	The planet a window shows under its page, if any (planning/sprint-33): the
	shell's side of the Planetarium (planetarium/: the renderer), a manager
	of the current one. One per window, as long as the page (the Shell owns
	it): the page can always ask. The page asks through the contract
	(ui/src/bridge/types.ts, `planetarium.*`), checked here where it
	arrives; whoever runs the window now (the game: its own world; the
	menu: a world code) says how a body is made from what the page asks
	(`serve`); with no one, nothing can be shown. Showing another body
	keeps the old one on screen until the new one's coarse layer is built,
	so the planet never blanks.

	It draws where the page leaves room for it (`viewport`, in page pixels;
	the whole window until it says).

	Back to the page: { kind: "planetarium", event: "picked", latitude,
	longitude, cell: { region, index, i, j, k } } for a click on the body.
*/
class Planetarium
{
public:
	// The body the page asks for (`body` as the page sent it; may be null).
	// Throws shell::Refused for one this window can't show.
	using Body = std::function<planetarium::Planetarium::Opts(const Json::Value &body)>;

	struct Opts
	{
		shell::Shell &shell;
	};

	explicit Planetarium(Opts opts); // registers the contract's verbs
	~Planetarium();                  // and takes them off

	// Who makes bodies from now (the menu, a game), hiding what was shown:
	// one window's planet doesn't follow into the next. Empty: no one. A
	// backdrop (a body as the page would ask for it) is shown now and
	// whenever the page hides the planet: the menu's universe, always there.
	void serve(Body body, std::optional<Json::Value> backdrop = std::nullopt);

	bool shown() const { return m_current != nullptr || m_next != nullptr; }
	// Shows a body directly (a key, a command line), as `planetarium.show` does.
	void show(planetarium::Planetarium::Opts body);
	void hide();
	// The system's moment shown: its day (planetarium::Planetarium::day), or
	// a world's date (days into it, its year's length: System::dayFor).
	void day(double days);
	void dayIn(double worldDays, int32_t worldYear);

	// The mouse where the page doesn't take it, while shown. Whether it was ours.
	bool handle(const SEvent &event);
	// A frame, while shown: the planet over the whole screen, the scene
	// already begun; a click taken since is sent to the page.
	void draw(video::IVideoDriver *driver);

private:
	Json::Value show(const Json::Value &intent);
	Json::Value focus(const Json::Value &intent);
	Json::Value markers(const Json::Value &intent);
	Json::Value viewport(const Json::Value &intent);
	Json::Value fly(const Json::Value &intent);
	Json::Value frame(const Json::Value &intent);
	// the body the page asked for, made, unless it's the one shown already
	void ask(const Json::Value &body);
	// a body's index in the system shown, by its id; Refused for none
	size_t bodyIndex(const planetarium::Planetarium &p, const Json::Value &id) const;

	Opts m_opts;
	Body m_body;
	std::optional<Json::Value> m_backdrop;
	Json::Value m_asked; // the body shown (or being made), as asked for
	std::unique_ptr<planetarium::Planetarium> m_current; // on screen
	std::unique_ptr<planetarium::Planetarium> m_next;    // being made ready to take its place
	std::vector<planetarium::MarkerLayer::Marker> m_markers; // the page's, kept across a swap
	// where the page leaves room for it (page pixels); none: the whole window
	struct Rect
	{
		double x, y, width, height;
	};
	std::optional<Rect> m_view;
	// what the page covers round the view's edges (page pixels): the planet
	// is framed in the rest
	struct Insets
	{
		double left = 0, right = 0, top = 0, bottom = 0;
	};
	Insets m_insets;
};

} // namespace shell
