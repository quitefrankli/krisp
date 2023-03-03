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

	class Tetrahedron : public Shape
	{
	public:
		Tetrahedron();
		Tetrahedron(Tetrahedron&&) noexcept = default;
		virtual ~Tetrahedron() override {};

		virtual bool check_collision(const Maths::Ray& ray) override;
	};

	class Icosahedron : public Shape
	{
	public:
		Icosahedron();
		Icosahedron(Icosahedron&&) noexcept = default;
		virtual ~Icosahedron() override {};

		virtual bool check_collision(const Maths::Ray& ray) override;
	};
}