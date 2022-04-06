#pragma once

#include "objects/object.hpp"


class ScaleGizmoObj : public Object
{
public:
	ScaleGizmoObj(const glm::vec3& original_axis);
	ScaleGizmoObj(ScaleGizmoObj&&) = delete;

	void point(const glm::vec3& start, const glm::vec3& end);

	virtual bool check_collision(const Maths::Ray& ray) override;

public:
	const float THICKNESS = 0.1f;
	const float BLOCK_LENGTH = 0.15f;
	const glm::vec3 original_axis;
};