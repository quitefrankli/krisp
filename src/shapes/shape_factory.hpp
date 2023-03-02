#pragma once

#include "shape.hpp"


class ShapeFactory
{
public:
	static Shape generate_cube();
	static Shape generate_sphere();
};