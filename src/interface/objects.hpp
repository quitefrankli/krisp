#pragma once

#include "objects/object.hpp"
#include "renderable/renderable.hpp"

class ECS;

class ScaleGizmoObj : public Object
{
public:
	ScaleGizmoObj(const glm::vec3& original_axis);
	ScaleGizmoObj(ScaleGizmoObj&&) = delete;
	static Renderable make_renderable();

	void point(ECS& ecs, const glm::vec3& start, const glm::vec3& end);

public:
	static constexpr float INITIAL_RADIUS = 0.1f;
	static constexpr float BLOCK_LENGTH = 0.15f;
	const glm::vec3 original_axis;
};
