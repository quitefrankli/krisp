#pragma once

#include "objects/object.hpp"


class LightSource : public Object
{
public:
	LightSource(const glm::vec3& color);

	glm::vec3 get_color() const { return color; }

	virtual const EPipelineType get_render_type() const override { return EPipelineType::LIGHT_SOURCE; }

private:
	// glm::vec3 light_direction; // directional light not supported for now
	glm::vec3 color;
};