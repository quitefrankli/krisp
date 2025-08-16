#include "entity_component_system/mesh_system.hpp"
#include "mesh_factory.hpp"
#include "mesh.hpp"
#include "mesh_maths.hpp"
#include "maths.hpp"

#include <stdexcept>
#include <optional>
#include <algorithm>
#include <map>


//
// Mesh IDs
//

MeshID MeshFactory::quad_id(EVertexType vertex_type)
{
	static std::optional<MeshID> cached_color_quad;
	static std::optional<MeshID> cached_tex_quad;

	auto& cache = vertex_type == EVertexType::COLOR ? cached_color_quad : cached_tex_quad;
	if (!cache.has_value())
	{
		cache = MeshSystem::add_permanent(quad(vertex_type));
	}

	return *cache;
}

MeshID MeshFactory::cube_id(EVertexType vertex_type)
{
	static std::optional<MeshID> cached_color_cube;
	static std::optional<MeshID> cached_tex_cube;

	auto& cache = vertex_type == EVertexType::COLOR ? cached_color_cube : cached_tex_cube;
	if (!cache.has_value())
	{
		cache = MeshSystem::add_permanent(cube(vertex_type));
	}

	return *cache;
}

MeshID MeshFactory::circle_id(EVertexType vertex_type, uint32_t nVertices)
{
	static std::map<std::pair<EVertexType, uint32_t>, MeshID> cached_circles;
	const auto pair = std::make_pair(vertex_type, nVertices);
	if (!cached_circles.contains(pair))
	{
		cached_circles[pair] = MeshSystem::add_permanent(circle(vertex_type, nVertices));
	}

	return cached_circles[pair];
}

MeshID MeshFactory::icosahedron_id(EVertexType vertex_type)
{
	static std::optional<MeshID> cached_color_icosahedron;
	static std::optional<MeshID> cached_tex_icosahedron;
	
	auto& cache = vertex_type == EVertexType::COLOR ? cached_color_icosahedron : cached_tex_icosahedron;
	if (!cache.has_value())
	{
		cache = MeshSystem::add_permanent(icosahedron(vertex_type));
	}

	return *cache;
}

MeshID MeshFactory::sphere_id(EVertexType vertex_type, GenerationMethod method, int nVertices)
{
	static std::map<std::pair<GenerationMethod, int>, MeshID> cached_color_sphere;
	const auto pair = std::make_pair(method, nVertices);
	if (!cached_color_sphere.contains(pair))
	{
		cached_color_sphere[pair] = MeshSystem::add_permanent(sphere(vertex_type, method, nVertices));
	}

	return cached_color_sphere[pair];
}

MeshID MeshFactory::cone_id(EVertexType vertex_type, uint32_t nVertices)
{
	static std::map<std::pair<EVertexType, uint32_t>, MeshID> cached_cones;
	const auto pair = std::make_pair(vertex_type, nVertices);
	if (!cached_cones.contains(pair))
	{
		cached_cones[pair] = MeshSystem::add_permanent(cone(vertex_type, nVertices));
	}

	return cached_cones[pair];
}

MeshID MeshFactory::cylinder_id(EVertexType vertex_type, uint32_t nVertices)
{
	static std::map<std::pair<EVertexType, uint32_t>, MeshID> cached_cylinders;
	const auto pair = std::make_pair(vertex_type, nVertices);
	if (!cached_cylinders.contains(pair))
	{
		cached_cylinders[pair] = MeshSystem::add_permanent(cylinder(vertex_type, nVertices));
	}

	return cached_cylinders[pair];
}

MeshID MeshFactory::arrow_id(float radius, uint32_t nVertices)
{
	static std::map<std::pair<float, uint32_t>, MeshID> cached_arrows;
	const auto pair = std::make_pair(radius, nVertices);
	if (!cached_arrows.contains(pair))
	{
		cached_arrows[pair] = MeshSystem::add_permanent(arrow(radius, nVertices));
	}

	return cached_arrows[pair];
}

MeshID MeshFactory::arc_id(uint32_t nSegments, float outer_radius, float inner_radius)
{
	static std::map<std::tuple<uint32_t, float, float>, MeshID> cached_arcs;
	const auto tuple = std::make_tuple(nSegments, outer_radius, inner_radius);
	if (!cached_arcs.contains(tuple))
	{
		cached_arcs[tuple] = MeshSystem::add_permanent(arc(nSegments, outer_radius, inner_radius));
	}

	return cached_arcs[tuple];
}


//
// Raw Mesh
//

MeshPtr MeshFactory::quad(EVertexType vertex_type)
{
	// notice how the position has +Y going up and -Y going down
	// while the UV has +V going down and -V going up
	// this is because we are not flipping the textures when we load them
	// instead we are loading them in, in read order (as opposed to opengl)

	// our coordinate system is LEFT handed:
	// +X = right
	// +Y = up
	// +Z = into screen/forwards
	// using LEFT hand, when thumb is parallel to the AXIS of rotation, then the
	// DIRECTION of rotation follows the curled fingers

	// {pos, texCoord, normal}
	const std::vector<uint32_t> indices { 0, 1, 2, 2, 3, 0 };

	switch (vertex_type)
	{
	case EVertexType::COLOR:
	{
		return std::make_unique<ColorMesh>(
			std::vector<SDS::ColorVertex>{
				SDS::ColorVertex{{-0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}},	// bottom left
				SDS::ColorVertex{{ 0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}}, // bottom right
				SDS::ColorVertex{{ 0.5f,  0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}}, // top right
				SDS::ColorVertex{{-0.5f,  0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}} 	// top left
			},
			indices);
	}
	case EVertexType::TEXTURE:
	{
		return std::make_unique<TexMesh>(
			std::vector<SDS::TexVertex>{
				SDS::TexVertex{{-0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}}, // bottom left
				SDS::TexVertex{{ 0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}}, // bottom right
				SDS::TexVertex{{ 0.5f,  0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}}, // top right
				SDS::TexVertex{{-0.5f,  0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}}	// top left
			},
			indices);
	}
	default:
		throw std::runtime_error("MeshFactory::quad: unknown EVertexType");
	}
}

template<typename VertexType, typename MeshType>
void add_cube_faces(
	std::vector<VertexType>& vertices,
	std::vector<uint32_t>& indices,
	const MeshType& template_face)
{
	const std::vector<glm::quat> rotators = {
		glm::quat{},
		glm::angleAxis(-Maths::PI * 0.5f, glm::vec3(0.0f, 1.0f, 0.0f)),
		glm::angleAxis(-Maths::PI, glm::vec3(0.0f, 1.0f, 0.0f)),
		glm::angleAxis(-Maths::PI * 1.5f, glm::vec3(0.0f, 1.0f, 0.0f)),
		glm::angleAxis(-Maths::PI * 2.0f, glm::vec3(0.0f, 1.0f, 0.0f)),
		glm::angleAxis(-Maths::PI * 0.5f, glm::vec3(-1.0f, 0.0f, 0.0f)),
		glm::angleAxis(-Maths::PI * 0.5f, glm::vec3(1.0f, 0.0f, 0.0f))
	};

	vertices.reserve(rotators.size() * template_face.get_num_unique_vertices());
	for (const auto& rotator : rotators)
	{
		const uint32_t index_offset = vertices.size();

		auto new_vertices = template_face.get_vertices();
		translate_vertices(new_vertices, glm::vec3(0.0f, 0.0f, 0.5f));
		transform_vertices(new_vertices, glm::mat4_cast(rotator));
		vertices.insert(
			vertices.end(),
			std::make_move_iterator(new_vertices.begin()),
			std::make_move_iterator(new_vertices.end()));

		for (auto index : template_face.get_indices())
		{
			indices.push_back(index + index_offset);
		}
	}
}

MeshPtr MeshFactory::cube(EVertexType vertex_type)
{
	const auto template_face = quad(vertex_type);
	switch (vertex_type)
	{
	case EVertexType::COLOR:
	{
		const auto& color_template_face = static_cast<ColorMesh&>(*template_face);
		std::vector<SDS::ColorVertex> vertices;
		std::vector<uint32_t> indices;

		add_cube_faces(vertices, indices, color_template_face);

		return std::make_unique<ColorMesh>(vertices, indices);
	}
	case EVertexType::TEXTURE:
	{
		const auto& tex_template_face = static_cast<TexMesh&>(*template_face);
		std::vector<SDS::TexVertex> vertices;
		std::vector<uint32_t> indices;

		add_cube_faces(vertices, indices, tex_template_face);

		return std::make_unique<TexMesh>(vertices, indices);
	}
	default:
		throw std::runtime_error("MeshFactory::cube: unknown EVertexType");
	}
}

MeshPtr MeshFactory::circle(EVertexType vertex_type, uint32_t nVertices)
{
	if (vertex_type != EVertexType::COLOR)
	{
		throw std::runtime_error("MeshFactory::circle: unsupported vertex type!");
	}

	std::vector<SDS::ColorVertex> vertices;
	std::vector<uint32_t> indices;

	for (int i = 0; i < nVertices; i++)
	{
		const float m = (float)i/nVertices * Maths::PI * 2.0f;
		vertices.emplace_back(SDS::ColorVertex{
			glm::vec3(Maths::cosf(m) * 0.5f, 0.0f, Maths::sinf(m) * 0.5f)});
	}
	vertices.emplace_back(SDS::ColorVertex{glm::vec3(0.0f)}); // center

	for (int i = 0; i < nVertices; i++)
	{
		indices.emplace_back(i);
		indices.emplace_back(nVertices);
		indices.emplace_back((i+1)%nVertices);
	}

	return std::make_unique<ColorMesh>(vertices, indices);
}

MeshPtr MeshFactory::icosahedron(EVertexType vertex_type)
{
	if (vertex_type != EVertexType::COLOR)
	{
		throw std::runtime_error("MeshFactory::icosahedron: unsupported vertex type!");
	}

	// https://schneide.blog/2016/07/15/generating-an-icosphere-in-c/
	const float X=.525731112119133606f/2.0f;
	const float Z=.850650808352039932f/2.0f;
	const float N=0.f;
	const glm::vec3 color{1.0f, 1.0f, 1.0f};
	
	std::vector<SDS::ColorVertex> vertices = {
		{glm::vec3{-X,N,Z}, color}, {glm::vec3{X,N,Z}, color}, 
		{glm::vec3{-X,N,-Z}, color}, {glm::vec3{X,N,-Z}, color},
		{glm::vec3{N,Z,X}, color}, {glm::vec3{N,Z,-X}, color}, 
		{glm::vec3{N,-Z,X}, color}, {glm::vec3{N,-Z,-X}, color},
		{glm::vec3{Z,X,N}, color}, {glm::vec3{-Z,X, N}, color}, 
		{glm::vec3{Z,-X,N}, color}, {glm::vec3{-Z,-X, N}, color}
	};
	
	std::vector<uint32_t> indices = {
		0, 4, 1, 0, 9, 4, 9, 5, 4, 4, 5, 8, 4, 8, 1, 
		8, 10, 1, 8, 3, 10, 5, 3, 8, 5, 2, 3, 2, 7, 3, 
		7, 10, 3, 7, 6, 10, 7, 11, 6, 11, 0, 6, 0, 1, 6, 
		6, 1, 10, 9, 0, 11, 9, 11, 2, 9, 2, 5, 7, 2, 11
	};

	std::ranges::reverse(indices);

	generate_normals(vertices, indices);

	return std::make_unique<ColorMesh>(vertices, indices);
}

MeshPtr MeshFactory::sphere(EVertexType vertex_type, GenerationMethod method, int nVertices)
{
	if (vertex_type != EVertexType::COLOR)
	{
		throw std::runtime_error("MeshFactory::sphere: unsupported vertex type!");
	}

	switch (method)
	{
	case GenerationMethod::UV_SPHERE:
		return generate_uv_sphere(nVertices);
	case GenerationMethod::ICO_SPHERE:
		return generate_ico_sphere(nVertices);
	default:
		throw std::runtime_error("MeshFactory::sphere: unknown GenerationMethod");
	}
}

MeshPtr MeshFactory::cone(EVertexType vertex_type, uint32_t nVertices)
{
	if (vertex_type != EVertexType::COLOR)
	{
		throw std::runtime_error("MeshFactory::cone: unsupported vertex type!");
	}

	ColorVertices vertices;
	VertexIndices indices;
	for (int i = 0; i < nVertices; i++)
	{
		float m = (float)i/nVertices * Maths::PI * 2.0f;
		vertices.emplace_back(SDS::ColorVertex{glm::vec3(cosf(m) * 0.5f, -0.5f, sinf(m) * 0.5f)});
	}
	vertices.emplace_back(SDS::ColorVertex{glm::vec3(0.0f, -0.5f, 0.0f)}); // centers
	vertices.emplace_back(SDS::ColorVertex{glm::vec3(0.0f, 0.5f, 0.0f)});

	for (int i = 0; i < nVertices; i++)
	{
		// side
		indices.emplace_back(nVertices+1);
		indices.emplace_back((i+1)%nVertices);
		indices.emplace_back(i);

		// bottom
		indices.emplace_back(i);
		indices.emplace_back((i+1)%nVertices);
		indices.emplace_back(nVertices);
	}

	generate_normals(vertices, indices);

	return std::make_unique<ColorMesh>(vertices, indices);
}

MeshPtr MeshFactory::cylinder(EVertexType vertex_type, uint32_t nVertices)
{
	if (vertex_type != EVertexType::COLOR)
	{
		throw std::runtime_error("MeshFactory::cone: unsupported vertex type!");
	}

	ColorVertices vertices;
	VertexIndices indices;
	const auto generate_vertices = [&vertices, nVertices](float y)
	{
		for (int i = 0; i < nVertices; i++)
		{
			const float m = (float)i/nVertices * Maths::PI * 2.0f;
			vertices.emplace_back(SDS::ColorVertex{glm::vec3(cosf(m) * 0.5f, y, sinf(m) * 0.5f)});
		}
	};

	generate_vertices(0.5f);
	generate_vertices(-0.5f); // edges
	vertices.emplace_back(SDS::ColorVertex{glm::vec3(0.0f, 0.5f, 0.0f)});
	vertices.emplace_back(SDS::ColorVertex{glm::vec3(0.0f, -0.5f, 0.0f)}); // centers

	for (int i = 0; i < nVertices; i++)
	{
		// top
		indices.emplace_back(i);
		indices.emplace_back(nVertices*2);
		indices.emplace_back((i+1)%(nVertices));

		// side
		indices.emplace_back(i);
		indices.emplace_back((i + 1)%nVertices);
		indices.emplace_back(i + nVertices);
		indices.emplace_back(i + nVertices);
		indices.emplace_back((i + 1)%nVertices);
		indices.emplace_back((i+1)%nVertices + nVertices);

		// bottom
		indices.emplace_back(i + nVertices);
		indices.emplace_back((i+1)%nVertices + nVertices);
		indices.emplace_back(nVertices*2+1);
	}

	generate_normals(vertices, indices);

	return std::make_unique<ColorMesh>(vertices, indices);
}

MeshPtr MeshFactory::arrow(float radius, uint32_t nVertices)
{
	const auto cylinder_mesh_ = cylinder(EVertexType::COLOR, nVertices);
	const auto cone_mesh_ = cone(EVertexType::COLOR, nVertices);
	const ColorMesh& cylinder_mesh = static_cast<ColorMesh&>(*cylinder_mesh_);
	const ColorMesh& cone_mesh = static_cast<ColorMesh&>(*cone_mesh_);

	glm::quat quat = glm::angleAxis(Maths::PI/2.0f, Maths::right_vec);

	glm::mat4 cylinder_transform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.4f, 0.0f));
	cylinder_transform = cylinder_transform * glm::scale(glm::mat4(1.0f), glm::vec3(radius*2.0f, 0.8f, radius*2.0f));
	cylinder_transform = glm::mat4_cast(quat) * cylinder_transform;
	auto cylinder_vertices = cylinder_mesh.get_vertices();
	transform_vertices(cylinder_vertices, cylinder_transform);

	glm::mat4 cone_transform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.9f, 0.0f));
	cone_transform = cone_transform * glm::scale(glm::mat4(1.0f), glm::vec3(0.3f, 0.2f, 0.3f));
	cone_transform = glm::mat4_cast(quat) * cone_transform;
	auto cone_vertices = cone_mesh.get_vertices();
	transform_vertices(cone_vertices, cone_transform);

	VertexIndices indices = cylinder_mesh.get_indices();
	const auto idx_offset = cylinder_mesh.get_num_unique_vertices();
	cylinder_vertices.insert(
		cylinder_vertices.end(),
		std::make_move_iterator(cone_vertices.begin()),
		std::make_move_iterator(cone_vertices.end()));
	indices.reserve(indices.size() + cone_mesh.get_indices().size());
	for (const auto index : cone_mesh.get_indices())
	{
		indices.push_back(index + idx_offset);
	}

	return std::make_unique<ColorMesh>(cylinder_vertices, indices);
}

MeshPtr MeshFactory::arc(uint32_t nSegments, float outer_radius, float inner_radius)
{
	ColorVertices vertices;
	VertexIndices indices;

	// generated via 2 concentric circles
	const float thickness = 0.02f; // 3d objects require a little thickness
	const float inc = Maths::PI/2.0f/(float)nSegments;

	// generate all vertices

	vertices.reserve(nSegments * 2);
	auto gen_vertex = [&](const float radius, const float y, const int i)
	{
		vertices.emplace_back(SDS::ColorVertex{
			radius * glm::vec3(Maths::sinf(inc * i), y, Maths::cosf(inc * i)),
			glm::vec3((float)i/(float)nSegments, 1.0f, 1.0f)});
	};
	for (int i = 0; i <= nSegments; i++)
		gen_vertex(outer_radius, thickness*0.5f, i);
	for (int i = 0; i <= nSegments; i++)
		gen_vertex(inner_radius, thickness*0.5f, i);
	for (int i = 0; i <= nSegments; i++)
		gen_vertex(outer_radius, -thickness*0.5f, i);
	for (int i = 0; i <= nSegments; i++)
		gen_vertex(inner_radius, -thickness*0.5f, i);

	const int outer_top_offset = 0;
	const int inner_top_offset = nSegments+1;
	const int outer_bot_offset = nSegments+nSegments+2;
	const int inner_bot_offset = nSegments+nSegments+nSegments+3;
	for (int i = 0; i < nSegments; i++)
	{
		// top
		indices.push_back(outer_top_offset + i);
		indices.push_back(inner_top_offset + i);
		indices.push_back(outer_top_offset + i + 1);
		indices.push_back(outer_top_offset + i + 1);
		indices.push_back(inner_top_offset + i);
		indices.push_back(inner_top_offset + i + 1);

		// bottom
		indices.push_back(outer_bot_offset + i);
		indices.push_back(outer_bot_offset + i + 1);
		indices.push_back(inner_bot_offset + i);
		indices.push_back(inner_bot_offset + i);
		indices.push_back(outer_bot_offset + i + 1);
		indices.push_back(inner_bot_offset + i + 1);

		// we don't actually need the sides given a thin enough thickness and single sided shading
	}

	// let default plane normal be the forward axis
	transform_vertices(vertices, glm::angleAxis(-Maths::PI/2.0f, glm::vec3(1.0f, 0.0f, 0.0f)));	
	generate_normals(vertices, indices);

	return std::make_unique<ColorMesh>(vertices, indices);
}

MeshPtr MeshFactory::generate_uv_sphere(int nVertices)
{
	ColorVertices vertices;
	VertexIndices indices;

	const int M = Maths::sqrtf(nVertices);
	const int N = M;
	const auto calculate_vertex = [&M, &N](float m, float n)
	{
		m = m / (float)M * Maths::PI;
		n = n / (float)N * Maths::PI;
		SDS::ColorVertex vertex;
		vertex.pos = {
			Maths::sinf(2.0f * n) * Maths::sinf(m),
			Maths::cosf(m),
			Maths::cosf(2.0f * n) * Maths::sinf(m)
		};
		vertex.pos /= 2.0f;

		return vertex;
	};

	// generate all vertices of sphere
	vertices.reserve(M*N);
	indices.reserve(M*N*8);

	for (int m = 0; m <= M; m++)
	{
		for (int n = 0; n < N; n++)
		{
			vertices.push_back(calculate_vertex(m, n));
		}
	}
	
	auto get_index = [&](int m, int n)
	{
		return m*N + n;
	};

	// edge case first row
	for (int n = 0; n < N; n++)
	{
		const int m = 1;
		indices.push_back(0);
		indices.push_back(get_index(m, n));
		indices.push_back(get_index(m, (n+1)%N));
	}
	for (int m = 1; m < M-1; m++)
	{
		for (int n = 0; n < N; n++)
		{
			indices.push_back(get_index(m, n));
			indices.push_back(get_index(m+1, n));
			// mod N here due to edge case at last column of a row
			indices.push_back(get_index(m+1, (n+1)%N));
			indices.push_back(get_index(m, n));
			indices.push_back(get_index(m+1, (n+1)%N));
			indices.push_back(get_index(m, (n+1)%N));
		}
	}
	// edge case last row
	for (int n = 0; n < N; n++)
	{
		const int m = M-1;
		indices.push_back(get_index(m, n));
		indices.push_back(vertices.size()-1);
		indices.push_back(get_index(m, (n+1)%N));
	}

	generate_normals(vertices, indices);

	return std::make_unique<ColorMesh>(vertices, indices);
}

MeshPtr MeshFactory::generate_ico_sphere(int nVertices)
{
	auto ico_ = icosahedron(EVertexType::COLOR);
	auto& ico = static_cast<ColorMesh&>(*ico_);

	ColorVertices vertices = ico.get_vertices();
	VertexIndices indices = ico.get_indices();

	const int nVerticesPerFace = 3;
	const int nFaces = indices.size() / nVerticesPerFace;


	const auto color = glm::vec3(1.0f, 1.0f, 1.0f);

	// for each triangle subdivide it into 4 triangles
	while (vertices.size() <= nVertices)
	{
		for (int i = 0, indices_size = indices.size(); i < indices_size; i += 3)
		{
			// get the 3 vertices of the triangle
			const auto v1 = vertices[indices[i]].pos;
			const auto v2 = vertices[indices[i+1]].pos;
			const auto v3 = vertices[indices[i+2]].pos;

			// calculate the new vertices
			// these lie on the mid point of edges of the above triangle
			vertices.emplace_back(SDS::ColorVertex{glm::normalize(v1 + v2)*0.5f, color});
			vertices.emplace_back(SDS::ColorVertex{glm::normalize(v2 + v3)*0.5f, color});
			vertices.emplace_back(SDS::ColorVertex{glm::normalize(v3 + v1)*0.5f, color});

			const uint32_t i12 = vertices.size()-3;
			const uint32_t i23 = vertices.size()-2;
			const uint32_t i31 = vertices.size()-1;

			// add 3 of the 4 new triangles
			indices.insert(indices.end(), 
			{
				indices[i], i12, i31,
				i12, indices[i+1], i23,
				i31, i23, indices[i+2]
			});

			// replace original triangle with the 4th new one
			indices[i] = i12;
			indices[i+1] = i23;
			indices[i+2] = i31;
		}
	}
	
	generate_normals(vertices, indices);

	return std::make_unique<ColorMesh>(vertices, indices);
}
