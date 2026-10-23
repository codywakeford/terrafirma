// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <memory>

namespace video { class IVideoDriver; }

namespace shell {

// A generated world seen from orbit, drawn behind the transparent menu page.
class Orbit
{
public:
	explicit Orbit(video::IVideoDriver *driver);
	~Orbit();
	void draw(float seconds);

private:
	struct Impl;
	std::unique_ptr<Impl> m;
};

} // namespace shell
