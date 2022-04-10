#pragma once

#include "object.hpp"


class Cube : public Object
{
public:
	Cube();
	Cube(Cube&& cube) noexcept = default;
};

// note that this is procedurally generated and very slow
class Sphere : public Object
{
public:
	Sphere();
	Sphere(Sphere&& cube) noexcept = default;
};

class HollowCylinder : public Object
{
public:
	HollowCylinder();
	HollowCylinder(HollowCylinder&& hollow_cylinder) noexcept = default;

	// for tower of hanoi
	int pillar_index = 0;
};

class Cylinder : public Object
{
public:
	Cylinder();
	Cylinder(Cylinder&& cylinder) noexcept = default;

	// for tower of hanoi
	float content_height = 0.0f;
};

class Arrow : public Object
{
public:
	Arrow();
	Arrow(Arrow&&) = delete;

	void point(const glm::vec3& start, const glm::vec3& end);

	virtual bool check_collision(const Maths::Ray& ray) override;

public:
	const float RADIUS = 0.05f;
};

class Arc : public Object
{
public:
	Arc();
	Arc(Arc&&) = delete;

	virtual bool check_collision(const Maths::Ray& ray) override;

	const float outer_radius = 1.0f;
	const float inner_radius = 0.8f;
};