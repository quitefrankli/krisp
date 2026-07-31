#pragma once

#include "object.hpp"
#include "renderable/renderable.hpp"

class ECS;

class Arrow : public Object
{
public:
	Arrow() = default;
	Arrow(Arrow&&) = delete;
	static Renderable make_renderable();

	void point(ECS& ecs, const glm::vec3& start, const glm::vec3& end);

public:
	static constexpr float INITIAL_RADIUS = 0.05f;
};

class ArcObject : public Object
{
public:
	ArcObject() = default;
	ArcObject(ArcObject&&) = delete;
	static Renderable make_renderable();

	static constexpr float INITIAL_OUTER_RAIUS = 1.0f;
	static constexpr float INITIAL_INNER_RADIUS = 0.8f;
};
