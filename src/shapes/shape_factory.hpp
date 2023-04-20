#pragma once

#include "shape.hpp"


class ShapeFactory
{
public:
	enum class GenerationMethod
	{
		// sphere
		UV_SPHERE,
		ICO_SPHERE
	};


	static Shape generate_cube();

	static Shape generate_sphere(GenerationMethod method = GenerationMethod::UV_SPHERE, int nVertices = 512);

private:
	static Shape generate_uv_sphere(int nVertices);
	static Shape generate_ico_sphere(int nVertices);
};