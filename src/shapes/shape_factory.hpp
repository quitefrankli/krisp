#pragma once

#include "shape.hpp"


class ShapeFactory
{
public:
	enum class EVertexType
	{
		COLOR,
		TEXTURE
	};

	enum class GenerationMethod
	{
		// sphere
		UV_SPHERE,
		ICO_SPHERE
	};

	using RetType = std::unique_ptr<Shape>;

	static RetType quad(EVertexType vertex_type = EVertexType::COLOR);
	static RetType cube(EVertexType vertex_type = EVertexType::COLOR);
	static RetType circle(EVertexType vertex_type = EVertexType::COLOR, uint32_t nVertices = 10);
	static RetType icosahedron(EVertexType vertex_type = EVertexType::COLOR);
	static RetType sphere(
		EVertexType vertex_type = EVertexType::COLOR, 
		GenerationMethod method = GenerationMethod::UV_SPHERE, 
		int nVertices = 512);
	static RetType cone(EVertexType vertex_type = EVertexType::COLOR, uint32_t nVertices = 10);
	static RetType cylinder(EVertexType vertex_type = EVertexType::COLOR, uint32_t nVertices = 10);

private:
	static RetType generate_uv_sphere(int nVertices);
	static RetType generate_ico_sphere(int nVertices);
};