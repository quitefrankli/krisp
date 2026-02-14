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
	static constexpr float INITIAL_RADIUS = 0.05f;
};

class ArcObject : public Object
{
public:
	ArcObject();
	ArcObject(ArcObject&&) = delete;

	virtual bool check_collision(const Maths::Ray& ray) override;
	virtual bool check_collision(const Maths::Ray& ray, glm::vec3& intersection) const override;

	static constexpr float INITIAL_OUTER_RAIUS = 1.0f;
	static constexpr float INITIAL_INNER_RADIUS = 0.8f;
};
