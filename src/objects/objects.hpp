#pragma once

#include "object.hpp"


class Arrow : public Object
{
public:
	Arrow();
	Arrow(const glm::vec3& start, const glm::vec3& end);
	Arrow(Arrow&&) = delete;

	void point(const glm::vec3& start, const glm::vec3& end);

public:
	static constexpr float INITIAL_RADIUS = 0.05f;
};

class ArcObject : public Object
{
public:
	ArcObject();
	ArcObject(ArcObject&&) = delete;

	static constexpr float INITIAL_OUTER_RAIUS = 1.0f;
	static constexpr float INITIAL_INNER_RADIUS = 0.8f;
};
