// The sphere lens's axes as every vertex shader gets them (src/client/shader.cpp).
#include "planet/planet.h"
#include <iostream>

int main()
{
	std::cout << planet::Planet::glslAxes();
}
