#pragma once


#include <objects/object.hpp>


// Planet is a misnomer as this represents any celestial body, be it a planet, moon, asteroid, etc.
class Planet : public Object
{
public:
	Planet() = default;
	Planet(const Renderable& renderable) : Object(renderable) {}
	Planet(const std::vector<Renderable>& renderables) : Object(renderables) {}

	glm::quat
};