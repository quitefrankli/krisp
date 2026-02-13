#pragma once

#include <objects/object.hpp>

class Planet : public Object
{
public:
	Planet() = default;
	Planet(const Renderable& renderable) : Object(renderable) {}
	Planet(const std::vector<Renderable>& renderables) : Object(renderables) {}
};