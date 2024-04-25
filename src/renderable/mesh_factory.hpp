#pragma once

#include "identifications.hpp"
#include "mesh.hpp"


class MeshFactory
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

public:
	static MeshID quad_id(EVertexType vertex_type = EVertexType::COLOR);
	static MeshID cube_id(EVertexType vertex_type = EVertexType::COLOR);
	static MeshID circle_id(EVertexType vertex_type = EVertexType::COLOR, uint32_t nVertices = 8);
	static MeshID icosahedron_id(EVertexType vertex_type = EVertexType::COLOR);
	static MeshID sphere_id(
		EVertexType vertex_type = EVertexType::COLOR, 
		GenerationMethod method = GenerationMethod::UV_SPHERE, 
		int nVertices = 512);
	static MeshID cone_id(EVertexType vertex_type = EVertexType::COLOR, uint32_t nVertices = 8);
	static MeshID cylinder_id(EVertexType vertex_type = EVertexType::COLOR, uint32_t nVertices = 8);
	static MeshID arrow_id(float radius = 0.05f, uint32_t nVertices = 8);
	static MeshID arc_id(uint32_t nSegments = 10, float outer_radius = 1.0f, float inner_radius = 0.8f);

public:
	static MeshPtr quad(EVertexType vertex_type = EVertexType::COLOR);
	static MeshPtr cube(EVertexType vertex_type = EVertexType::COLOR);
	static MeshPtr circle(EVertexType vertex_type = EVertexType::COLOR, uint32_t nVertices = 8);
	static MeshPtr icosahedron(EVertexType vertex_type = EVertexType::COLOR);
	static MeshPtr sphere(
		EVertexType vertex_type = EVertexType::COLOR, 
		GenerationMethod method = GenerationMethod::UV_SPHERE, 
		int nVertices = 512);
	static MeshPtr cone(EVertexType vertex_type = EVertexType::COLOR, uint32_t nVertices = 8);
	static MeshPtr cylinder(EVertexType vertex_type = EVertexType::COLOR, uint32_t nVertices = 8);
	static MeshPtr arrow(float radius = 0.05f, uint32_t nVertices = 8);
	static MeshPtr arc(uint32_t nSegments = 10, float outer_radius = 1.0f, float inner_radius = 0.8f);

private:
	static MeshPtr generate_uv_sphere(int nVertices);
	static MeshPtr generate_ico_sphere(int nVertices);
};