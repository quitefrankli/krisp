#pragma once

#include "shape.hpp"


namespace Shapes
{
	class Square : public Shape
	{
	public:
		Square();
		Square(Square&&) noexcept = default;
	};

	class Circle : public Shape
	{
	public:
		Circle(int nVertices, glm::vec3 color = {0.0f, 1.0f, 0.0f});
		Circle(Circle&&) noexcept = default;
	};

	class Cylinder : public Shape
	{
	public:
		Cylinder(int nVertices, glm::vec3 color = {0.0f, 1.0f, 0.0f});
		Cylinder(Cylinder&&) noexcept = default;
	};

	class Cone : public Shape
	{
	public:
		Cone(int nVertices, glm::vec3 color = {0.0f, 1.0f, 0.0f});
		Cone(Cone&&) noexcept = default;

		virtual bool check_collision(Maths::Ray& ray) override;
	};
}