#include "shape_factory.hpp"
#include "shapes/shapes.hpp"


Shape ShapeFactory::generate_cube()
{
	Shape new_shape;

	auto add_face = [&](const glm::quat& rotator)
	{
		const int index_offset = new_shape.vertices.size();
		Shapes::Square front;
		// glm::mat4 idt(1.0f);
		// glm::translate(idt, )
		front.translate_vertices(glm::vec3(0.0f, 0.0f, 0.5f));
		front.transform_vertices(glm::mat4_cast(rotator));
		new_shape.vertices.insert(new_shape.vertices.end(),
						std::make_move_iterator(front.vertices.begin()),
						std::make_move_iterator(front.vertices.end()));
		for (auto index : front.indices)
		{
			new_shape.indices.push_back(index + index_offset);
		}
	};
	
	new_shape.vertices.reserve(6*6);
	add_face(glm::quat{});
	add_face(glm::angleAxis(-Maths::PI * 0.5f, glm::vec3(0.0f, 1.0f, 0.0f)));
	add_face(glm::angleAxis(-Maths::PI, glm::vec3(0.0f, 1.0f, 0.0f)));
	add_face(glm::angleAxis(-Maths::PI * 1.5f, glm::vec3(0.0f, 1.0f, 0.0f)));
	add_face(glm::angleAxis(-Maths::PI * 2.0f, glm::vec3(0.0f, 1.0f, 0.0f)));
	add_face(glm::angleAxis(-Maths::PI * 0.5f, glm::vec3(-1.0f, 0.0f, 0.0f)));
	add_face(glm::angleAxis(-Maths::PI * 0.5f, glm::vec3(1.0f, 0.0f, 0.0f)));

	// deduplicate_vertices(); // TODO: do this
	new_shape.generate_normals();

	return new_shape;
}

Shape ShapeFactory::generate_sphere(GenerationMethod method, int nVertices)
{
	switch (method)
	{
	case GenerationMethod::UV_SPHERE:
		return generate_uv_sphere(nVertices);
	case GenerationMethod::ICO_SPHERE:
		return generate_ico_sphere(nVertices);
	default:
		throw std::runtime_error("Invalid generation method");
	}
}

Shape ShapeFactory::generate_uv_sphere(int nVertices)
{
	const int M = std::sqrtf(nVertices);
	const int N = M;
	const auto calculate_vertex = [&M, &N](float m, float n)
	{
		m = m / (float)M * Maths::PI;
		n = n / (float)N * Maths::PI;
		Vertex vertex;
		vertex.pos = {
			sinf(2.0f * n) * sinf(m),
			cosf(m),
			cosf(2.0f * n) * sinf(m)
		};
		vertex.pos /= 2.0f;
		vertex.color = {
			sinf(m*2.0f)/2.0f+0.5f, 1.0f, sinf(n*2.0f)/2.0f+0.5f
		};
		return vertex;
	};

	// generate all vertices of sphere
	Shape shape;
	shape.vertices.reserve(M*N);
	shape.indices.reserve(M*N*8);

	for (int m = 0; m <= M; m++)
	{
		for (int n = 0; n < N; n++)
		{
			shape.vertices.push_back(calculate_vertex(m, n));
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
		shape.indices.push_back(0);
		shape.indices.push_back(get_index(m, n));
		shape.indices.push_back(get_index(m, (n+1)%N));
	}
	for (int m = 1; m < M-1; m++)
	{
		for (int n = 0; n < N; n++)
		{
			shape.indices.push_back(get_index(m, n));
			shape.indices.push_back(get_index(m+1, n));
			// mod N here due to edge case at last column of a row
			shape.indices.push_back(get_index(m+1, (n+1)%N));
			shape.indices.push_back(get_index(m, n));
			shape.indices.push_back(get_index(m+1, (n+1)%N));
			shape.indices.push_back(get_index(m, (n+1)%N));
		}
	}
	// edge case last row
	for (int n = 0; n < N; n++)
	{
		const int m = M-1;
		shape.indices.push_back(get_index(m, n));
		shape.indices.push_back(shape.vertices.size()-1);
		shape.indices.push_back(get_index(m, (n+1)%N));
	}

	shape.generate_normals();

	return shape;
}

Shape ShapeFactory::generate_ico_sphere(int nVertices)
{
	Shapes::Icosahedron icosahedron;
	Shape new_shape;
	new_shape.vertices = std::move(icosahedron.vertices);
	new_shape.indices = std::move(icosahedron.indices);

	const int nVerticesPerFace = 3;
	const int nFaces = new_shape.indices.size() / nVerticesPerFace;


	const auto color = glm::vec3(1.0f, 1.0f, 1.0f);

	// for each triangle subdivide it into 4 triangles
	while (new_shape.vertices.size() <= nVertices)
	{
		for (int i = 0, indices_size = new_shape.indices.size(); i < indices_size; i += 3)
		{
			// get the 3 new_shape.vertices of the triangle
			const auto v1 = new_shape.vertices[new_shape.indices[i]].pos;
			const auto v2 = new_shape.vertices[new_shape.indices[i+1]].pos;
			const auto v3 = new_shape.vertices[new_shape.indices[i+2]].pos;

			// calculate the new new_shape.vertices
			// these lie on the mid point of edges of the above triangle
			new_shape.vertices.emplace_back(glm::normalize(v1 + v2), color);
			new_shape.vertices.emplace_back(glm::normalize(v2 + v3), color);
			new_shape.vertices.emplace_back(glm::normalize(v3 + v1), color);

			const uint32_t i12 = new_shape.vertices.size()-3;
			const uint32_t i23 = new_shape.vertices.size()-2;
			const uint32_t i31 = new_shape.vertices.size()-1;

			// add 3 of the 4 new triangles
			new_shape.indices.insert(new_shape.indices.end(), 
			{
				new_shape.indices[i], i12, i31,
				i12, new_shape.indices[i+1], i23,
				i31, i23, new_shape.indices[i+2]
			});

			// replace original triangle with the 4th new one
			new_shape.indices[i] = i12;
			new_shape.indices[i+1] = i23;
			new_shape.indices[i+2] = i31;
		}
	}
	
	new_shape.generate_normals();

	return new_shape;
}
