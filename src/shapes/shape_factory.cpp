#include "shape_factory.hpp"

#include <stdexcept>


ShapeFactory::RetType ShapeFactory::quad(EVertexType vertex_type)
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

	std::unique_ptr<Shape> shape;

	switch (vertex_type)
	{
	case EVertexType::COLOR:
		shape = std::make_unique<ColorShape>();
		shape->as<ColorShape>().get_vertices() = {
			SDS::ColorVertex{{-0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}},	// bottom left
			SDS::ColorVertex{{ 0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}}, // bottom right
			SDS::ColorVertex{{ 0.5f,  0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}}, // top right
			SDS::ColorVertex{{-0.5f,  0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}} 	// top left
		};
		break;
	case EVertexType::TEXTURE:
		shape = std::make_unique<TexShape>();
		shape->as<TexShape>().get_vertices() = {
			SDS::TexVertex{{-0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}}, // bottom left
			SDS::TexVertex{{ 0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}}, // bottom right
			SDS::TexVertex{{ 0.5f,  0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}}, // top right
			SDS::TexVertex{{-0.5f,  0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}}	// top left
		};
		break;
	default:
		throw std::runtime_error("ShapeFactory::quad: unknown EVertexType");
		break;
	}

	shape->get_indices() = {0, 1, 2, 2, 3, 0};

	return shape;
}

ShapeFactory::RetType ShapeFactory::cube(EVertexType vertex_type)
{
	if (vertex_type != EVertexType::COLOR && vertex_type != EVertexType::TEXTURE)
	{
		throw std::runtime_error("ShapeFactory::cube: unsupported vertex type!");
	}

	const RetType template_face = quad(vertex_type);
	std::unique_ptr<Shape> new_shape;
	if (vertex_type == EVertexType::COLOR)
	{
		new_shape = std::make_unique<ColorShape>();
		new_shape->as<ColorShape>().get_vertices().reserve(6 * template_face->get_num_unique_vertices());
		new_shape->as<ColorShape>().get_indices().reserve(6 * template_face->get_num_vertex_indices());
	} else
	{
		new_shape = std::make_unique<TexShape>();
		new_shape->as<TexShape>().get_vertices().reserve(6 * template_face->get_num_unique_vertices());
		new_shape->as<TexShape>().get_indices().reserve(6 * template_face->get_num_vertex_indices());
	}

	const auto add_face = [&new_shape, &template_face, vertex_type](const glm::quat& rotator) -> void
	{
		const uint32_t index_offset = new_shape->get_num_unique_vertices();

		if (vertex_type == EVertexType::COLOR)
		{
			auto new_vertices = template_face->as<ColorShape>().get_vertices();
			Shape::translate_vertices(new_vertices, glm::vec3(0.0f, 0.0f, 0.5f));
			Shape::transform_vertices(new_vertices, glm::mat4_cast(rotator));
			new_shape->as<ColorShape>().get_vertices().insert(
				new_shape->as<ColorShape>().get_vertices().end(),
				std::make_move_iterator(new_vertices.begin()),
				std::make_move_iterator(new_vertices.end()));
		} else 
		{
			auto new_vertices = template_face->as<TexShape>().get_vertices();
			Shape::translate_vertices(new_vertices, glm::vec3(0.0f, 0.0f, 0.5f));
			Shape::transform_vertices(new_vertices, glm::mat4_cast(rotator));
			new_shape->as<TexShape>().get_vertices().insert(
				new_shape->as<TexShape>().get_vertices().end(),
				std::make_move_iterator(new_vertices.begin()),
				std::make_move_iterator(new_vertices.end()));			
		}

		for (auto index : template_face->get_indices())
		{
			new_shape->get_indices().push_back(index + index_offset);
		}
	};
	
	add_face(glm::quat{});
	add_face(glm::angleAxis(-Maths::PI * 0.5f, glm::vec3(0.0f, 1.0f, 0.0f)));
	add_face(glm::angleAxis(-Maths::PI, glm::vec3(0.0f, 1.0f, 0.0f)));
	add_face(glm::angleAxis(-Maths::PI * 1.5f, glm::vec3(0.0f, 1.0f, 0.0f)));
	add_face(glm::angleAxis(-Maths::PI * 2.0f, glm::vec3(0.0f, 1.0f, 0.0f)));
	add_face(glm::angleAxis(-Maths::PI * 0.5f, glm::vec3(-1.0f, 0.0f, 0.0f)));
	add_face(glm::angleAxis(-Maths::PI * 0.5f, glm::vec3(1.0f, 0.0f, 0.0f)));

	return new_shape;
}

ShapeFactory::RetType ShapeFactory::circle(EVertexType vertex_type, uint32_t nVertices)
{
	if (vertex_type != EVertexType::COLOR)
	{
		throw std::runtime_error("ShapeFactory::circle: unsupported vertex type!");
	}

	ColorShape shape;
	for (int i = 0; i < nVertices; i++)
	{
		float m = (float)i/nVertices * Maths::PI * 2.0f;
		shape.get_vertices().emplace_back(SDS::ColorVertex{glm::vec3(std::cosf(m) * 0.5f, 0.0f, std::sinf(m) * 0.5f)});
	}
	shape.get_vertices().emplace_back(SDS::ColorVertex{glm::vec3(0.0f)}); // center

	for (int i = 0; i < nVertices; i++)
	{
		shape.get_indices().emplace_back(i);
		shape.get_indices().emplace_back(nVertices);
		shape.get_indices().emplace_back((i+1)%nVertices);
	}

	return std::make_unique<ColorShape>(std::move(shape));
}

ShapeFactory::RetType ShapeFactory::icosahedron(EVertexType vertex_type)
{
	if (vertex_type != EVertexType::COLOR)
	{
		throw std::runtime_error("ShapeFactory::icosahedron: unsupported vertex type!");
	}

	// https://schneide.blog/2016/07/15/generating-an-icosphere-in-c/
	const float X=.525731112119133606f/2.0f;
	const float Z=.850650808352039932f/2.0f;
	const float N=0.f;
	const glm::vec3 color{1.0f, 1.0f, 1.0f};
	ColorShape shape;
	shape.get_vertices() = {
		{glm::vec3{-X,N,Z}, color}, {glm::vec3{X,N,Z}, color}, 
		{glm::vec3{-X,N,-Z}, color}, {glm::vec3{X,N,-Z}, color},
		{glm::vec3{N,Z,X}, color}, {glm::vec3{N,Z,-X}, color}, 
		{glm::vec3{N,-Z,X}, color}, {glm::vec3{N,-Z,-X}, color},
		{glm::vec3{Z,X,N}, color}, {glm::vec3{-Z,X, N}, color}, 
		{glm::vec3{Z,-X,N}, color}, {glm::vec3{-Z,-X, N}, color}
	};
	
	shape.get_indices() = {
		0, 4, 1, 0, 9, 4, 9, 5, 4, 4, 5, 8, 4, 8, 1, 
		8, 10, 1, 8, 3, 10, 5, 3, 8, 5, 2, 3, 2, 7, 3, 
		7, 10, 3, 7, 6, 10, 7, 11, 6, 11, 0, 6, 0, 1, 6, 
		6, 1, 10, 9, 0, 11, 9, 11, 2, 9, 2, 5, 7, 2, 11
	};

	std::reverse(shape.get_indices().begin(), shape.get_indices().end());

	shape.generate_normals();

	return std::make_unique<ColorShape>(std::move(shape));
}

ShapeFactory::RetType ShapeFactory::sphere(EVertexType vertex_type, GenerationMethod method, int nVertices)
{
	if (vertex_type != EVertexType::COLOR)
	{
		throw std::runtime_error("ShapeFactory::sphere: unsupported vertex type!");
	}

	switch (method)
	{
	case GenerationMethod::UV_SPHERE:
		return generate_uv_sphere(nVertices);
	case GenerationMethod::ICO_SPHERE:
		return generate_ico_sphere(nVertices);
	default:
		throw std::runtime_error("ShapeFactory::sphere: unknown GenerationMethod");
	}
}

ShapeFactory::RetType ShapeFactory::cone(EVertexType vertex_type, uint32_t nVertices)
{
	if (vertex_type != EVertexType::COLOR)
	{
		throw std::runtime_error("ShapeFactory::cone: unsupported vertex type!");
	}

	ColorShape shape;
	for (int i = 0; i < nVertices; i++)
	{
		float m = (float)i/nVertices * Maths::PI * 2.0f;
		shape.get_vertices().emplace_back(SDS::ColorVertex{glm::vec3(cosf(m) * 0.5f, -0.5f, sinf(m) * 0.5f)});
	}
	shape.get_vertices().emplace_back(SDS::ColorVertex{glm::vec3(0.0f, -0.5f, 0.0f)}); // centers
	shape.get_vertices().emplace_back(SDS::ColorVertex{glm::vec3(0.0f, 0.5f, 0.0f)});

	for (int i = 0; i < nVertices; i++)
	{
		// side
		shape.get_indices().emplace_back(nVertices+1);
		shape.get_indices().emplace_back((i+1)%nVertices);
		shape.get_indices().emplace_back(i);

		// bottom
		shape.get_indices().emplace_back(i);
		shape.get_indices().emplace_back((i+1)%nVertices);
		shape.get_indices().emplace_back(nVertices);
	}

	shape.generate_normals();

	return std::make_unique<ColorShape>(std::move(shape));
}

ShapeFactory::RetType ShapeFactory::cylinder(EVertexType vertex_type, uint32_t nVertices)
{
	if (vertex_type != EVertexType::COLOR)
	{
		throw std::runtime_error("ShapeFactory::cone: unsupported vertex type!");
	}

	ColorShape shape;
	const auto generate_vertices = [&shape, nVertices](float y)
	{
		for (int i = 0; i < nVertices; i++)
		{
			const float m = (float)i/nVertices * Maths::PI * 2.0f;
			shape.get_vertices().emplace_back(SDS::ColorVertex{glm::vec3(cosf(m) * 0.5f, y, sinf(m) * 0.5f)});
		}
	};

	generate_vertices(0.5f);
	generate_vertices(-0.5f); // edges
	shape.get_vertices().emplace_back(SDS::ColorVertex{glm::vec3(0.0f, 0.5f, 0.0f)});
	shape.get_vertices().emplace_back(SDS::ColorVertex{glm::vec3(0.0f, -0.5f, 0.0f)}); // centers

	for (int i = 0; i < nVertices; i++)
	{
		// top
		shape.get_indices().emplace_back(i);
		shape.get_indices().emplace_back(nVertices*2);
		shape.get_indices().emplace_back((i+1)%(nVertices));

		// side
		shape.get_indices().emplace_back(i);
		shape.get_indices().emplace_back((i + 1)%nVertices);
		shape.get_indices().emplace_back(i + nVertices);
		shape.get_indices().emplace_back(i + nVertices);
		shape.get_indices().emplace_back((i + 1)%nVertices);
		shape.get_indices().emplace_back((i+1)%nVertices + nVertices);

		// bottom
		shape.get_indices().emplace_back(i + nVertices);
		shape.get_indices().emplace_back((i+1)%nVertices + nVertices);
		shape.get_indices().emplace_back(nVertices*2+1);
	}

	shape.generate_normals();

	return std::make_unique<ColorShape>(std::move(shape));
}

ShapeFactory::RetType ShapeFactory::arrow(float radius, uint32_t nVertices)
{
	auto cylinder = ShapeFactory::cylinder(ShapeFactory::EVertexType::COLOR, nVertices);
	auto cone = ShapeFactory::cone(ShapeFactory::EVertexType::COLOR, nVertices);

	glm::quat quat = glm::angleAxis(Maths::PI/2.0f, Maths::right_vec);

	glm::mat4 cylinder_transform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.4f, 0.0f));
	cylinder_transform = cylinder_transform * glm::scale(glm::mat4(1.0f), glm::vec3(radius*2.0f, 0.8f, radius*2.0f));
	cylinder_transform = glm::mat4_cast(quat) * cylinder_transform;
	cylinder->transform_vertices(cylinder_transform);

	glm::mat4 cone_transform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.9f, 0.0f));
	cone_transform = cone_transform * glm::scale(glm::mat4(1.0f), glm::vec3(0.3f, 0.2f, 0.3f));
	cone_transform = glm::mat4_cast(quat) * cone_transform;
	cone->transform_vertices(cone_transform);

	const auto idx_offset = cylinder->get_num_unique_vertices();
	ColorShape& color_shape = cylinder->as<ColorShape>();
	color_shape.get_vertices().insert(
		color_shape.get_vertices().end(),
		std::make_move_iterator(cone->as<ColorShape>().get_vertices().begin()),
		std::make_move_iterator(cone->as<ColorShape>().get_vertices().end()));
	for (const auto index : cone->get_indices())
	{
		color_shape.get_indices().push_back(index + idx_offset);
	}

	return std::move(cylinder);
}

ShapeFactory::RetType ShapeFactory::generate_uv_sphere(int nVertices)
{
	const int M = std::sqrtf(nVertices);
	const int N = M;
	const auto calculate_vertex = [&M, &N](float m, float n)
	{
		m = m / (float)M * Maths::PI;
		n = n / (float)N * Maths::PI;
		SDS::ColorVertex vertex;
		vertex.pos = {
			std::sinf(2.0f * n) * std::sinf(m),
			std::cosf(m),
			std::cosf(2.0f * n) * std::sinf(m)
		};
		vertex.pos /= 2.0f;

		return vertex;
	};

	// generate all vertices of sphere
	ColorShape shape;
	shape.get_vertices().reserve(M*N);
	shape.get_indices().reserve(M*N*8);

	for (int m = 0; m <= M; m++)
	{
		for (int n = 0; n < N; n++)
		{
			shape.get_vertices().push_back(calculate_vertex(m, n));
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
		shape.get_indices().push_back(0);
		shape.get_indices().push_back(get_index(m, n));
		shape.get_indices().push_back(get_index(m, (n+1)%N));
	}
	for (int m = 1; m < M-1; m++)
	{
		for (int n = 0; n < N; n++)
		{
			shape.get_indices().push_back(get_index(m, n));
			shape.get_indices().push_back(get_index(m+1, n));
			// mod N here due to edge case at last column of a row
			shape.get_indices().push_back(get_index(m+1, (n+1)%N));
			shape.get_indices().push_back(get_index(m, n));
			shape.get_indices().push_back(get_index(m+1, (n+1)%N));
			shape.get_indices().push_back(get_index(m, (n+1)%N));
		}
	}
	// edge case last row
	for (int n = 0; n < N; n++)
	{
		const int m = M-1;
		shape.get_indices().push_back(get_index(m, n));
		shape.get_indices().push_back(shape.get_vertices().size()-1);
		shape.get_indices().push_back(get_index(m, (n+1)%N));
	}

	shape.generate_normals();

	Material mat;
	mat.material_data.diffuse = glm::vec3(0.0f, 1.0f, 0.0f);
	shape.set_material(mat);

	return std::make_unique<ColorShape>(std::move(shape));
}

ShapeFactory::RetType ShapeFactory::generate_ico_sphere(int nVertices)
{
	auto new_shape = icosahedron(EVertexType::COLOR);
	auto& color_shape = new_shape->as<ColorShape>();

	const int nVerticesPerFace = 3;
	const int nFaces = color_shape.get_indices().size() / nVerticesPerFace;


	const auto color = glm::vec3(1.0f, 1.0f, 1.0f);

	// for each triangle subdivide it into 4 triangles
	while (color_shape.get_vertices().size() <= nVertices)
	{
		for (int i = 0, indices_size = color_shape.get_indices().size(); i < indices_size; i += 3)
		{
			// get the 3 color_shape.get_vertices() of the triangle
			const auto v1 = color_shape.get_vertices()[color_shape.get_indices()[i]].pos;
			const auto v2 = color_shape.get_vertices()[color_shape.get_indices()[i+1]].pos;
			const auto v3 = color_shape.get_vertices()[color_shape.get_indices()[i+2]].pos;

			// calculate the new color_shape.get_vertices()
			// these lie on the mid point of edges of the above triangle
			color_shape.get_vertices().emplace_back(SDS::ColorVertex{glm::normalize(v1 + v2)*0.5f, color});
			color_shape.get_vertices().emplace_back(SDS::ColorVertex{glm::normalize(v2 + v3)*0.5f, color});
			color_shape.get_vertices().emplace_back(SDS::ColorVertex{glm::normalize(v3 + v1)*0.5f, color});

			const uint32_t i12 = color_shape.get_vertices().size()-3;
			const uint32_t i23 = color_shape.get_vertices().size()-2;
			const uint32_t i31 = color_shape.get_vertices().size()-1;

			// add 3 of the 4 new triangles
			color_shape.get_indices().insert(color_shape.get_indices().end(), 
			{
				color_shape.get_indices()[i], i12, i31,
				i12, color_shape.get_indices()[i+1], i23,
				i31, i23, color_shape.get_indices()[i+2]
			});

			// replace original triangle with the 4th new one
			color_shape.get_indices()[i] = i12;
			color_shape.get_indices()[i+1] = i23;
			color_shape.get_indices()[i+2] = i31;
		}
	}
	
	color_shape.generate_normals();

	return new_shape;
}
