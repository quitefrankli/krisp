#pragma once

#include "shape.hpp"


class Square : public Shape
{
public:
	Square();
	Square(Square&& square) noexcept = default;
};

class Triangle : public Shape
{
public:
	Triangle();
	Triangle(Triangle&& triangle) noexcept = default;
};