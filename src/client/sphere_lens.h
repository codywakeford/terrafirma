// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "client/shader.h"
#include <string>

class Client;

/*
	The sphere lens (client/shaders/sphere/lens.glsl): its uniforms, set from
	the planet the server says the world is (Client::getPlanet; none on a
	flat world, and the lens is off).
*/
class SphereLensFactory : public IShaderUniformSetterFactory
{
public:
	explicit SphereLensFactory(Client *client) : m_client(client) {}
	IShaderUniformSetter *create(const std::string &name) override;

private:
	Client *m_client;
};
