#pragma once

#include "object.hpp"
#include "objects/object_interfaces/clickable.hpp"


class Arrow : public Object
{
public:
	Arrow();
	Arrow(Arrow&&) = delete;

	void point(const glm::vec3& start, const glm::vec3& end);

	virtual bool check_collision(const Maths::Ray& ray) override;
	virtual bool check_collision(const Maths::Ray& ray, glm::vec3& intersection) const override;

public:
	const float RADIUS = 0.05f;
};

class Arc : public Object
{
public:
	Arc();
	Arc(Arc&&) = delete;

	virtual bool check_collision(const Maths::Ray& ray) override;
	virtual bool check_collision(const Maths::Ray& ray, glm::vec3& intersection) const override;

	const float outer_radius = 1.0f;
	const float inner_radius = 0.8f;
};