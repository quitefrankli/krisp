#pragma once

#include "shape.hpp"


namespace Shapes
{
	class Square : public Shape
	{
	public:
		Square();
		Square(Square&&) noexcept = default;
		virtual ~Square() override {};
	};

	class Circle : public Shape
	{
	public:
		Circle(int nVertices, glm::vec3 color = {0.0f, 1.0f, 0.0f});
		Circle(Circle&&) noexcept = default;
		virtual ~Circle() override {};
	};

	class Cylinder : public Shape
	{
	public:
		Cylinder(int nVertices, glm::vec3 color = {0.0f, 1.0f, 0.0f});
		Cylinder(Cylinder&&) noexcept = default;
		virtual ~Cylinder() override {};
	};

	class Cone : public Shape
	{
	public:
		Cone(int nVertices, glm::vec3 color = {0.0f, 1.0f, 0.0f});
		Cone(Cone&&) noexcept = default;
		virtual ~Cone() override {};

		virtual bool check_collision(const Maths::Ray& ray) override;
	};

	class Cube : public Shape
	{
	public:
		Cube();
		Cube(Cube&&) noexcept = default;
		virtual ~Cube() override {};

		virtual bool check_collision(const Maths::Ray& ray) override;
	};

	class Sphere : public Shape
	{
	public:
		Sphere(int nVertices, glm::vec3 color = {0.0f, 1.0f, 0.0f});
		Sphere(Sphere&&) noexcept = default;
		virtual ~Sphere() override {};

		virtual bool check_collision(const Maths::Ray& ray) override;
	};
}