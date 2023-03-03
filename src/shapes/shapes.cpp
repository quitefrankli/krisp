#include "shapes.hpp"

#include "maths.hpp"

#include <glm/gtc/matrix_transform.hpp>


namespace Shapes
{
Square::Square()
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

	// {pos, color, texCoord, normal}
	vertices.push_back(Vertex({-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, 1.0f})); // bottom left
	vertices.push_back(Vertex({ 0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f})); // bottom right
	vertices.push_back(Vertex({ 0.5f,  0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, 1.0f})); // top right
	vertices.push_back(Vertex({-0.5f,  0.5f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, 1.0f})); // top left

	indices = 
	{
		0, 1, 2, 2, 3, 0
	};
}

Circle::Circle(int nVertices, glm::vec3 color)
{
	for (int i = 0; i < nVertices; i++)
	{
		float m = (float)i/nVertices * Maths::PI * 2.0f;
		vertices.emplace_back(
			glm::vec3(cosf(m) * 0.5f, 0.0f, sinf(m) * 0.5f),
			color
		);
	}
	vertices.emplace_back(glm::vec3(0.0f), color); // center

	for (int i = 0; i < nVertices; i++)
	{
		indices.emplace_back(i);
		indices.emplace_back(nVertices);
		indices.emplace_back((i+1)%nVertices);
	}
}

Cylinder::Cylinder(int nVertices, glm::vec3 color)
{
	auto generate_vertices = [&](float y)
	{
		for (int i = 0; i < nVertices; i++)
		{
			float m = (float)i/nVertices * Maths::PI * 2.0f;
			vertices.emplace_back(
				glm::vec3(cosf(m) * 0.5f, y, sinf(m) * 0.5f),
				color
			);
		}
	};

	generate_vertices(0.5f);
	generate_vertices(-0.5f); // edges
	vertices.emplace_back(glm::vec3(0.0f, 0.5f, 0.0f), color);
	vertices.emplace_back(glm::vec3(0.0f, -0.5f, 0.0f), color); // centers

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

	generate_normals();
}

Cone::Cone(int nVertices, glm::vec3 color)
{
	auto generate_vertices = [&](float y)
	{
		for (int i = 0; i < nVertices; i++)
		{
			float m = (float)i/nVertices * Maths::PI * 2.0f;
			vertices.emplace_back(
				glm::vec3(cosf(m) * 0.5f, y, sinf(m) * 0.5f),
				color
			);
		}
	};

	generate_vertices(-0.5f); // edges
	vertices.emplace_back(glm::vec3(0.0f, -0.5f, 0.0f), color); // centers
	vertices.emplace_back(glm::vec3(0.0f, 0.5f, 0.0f), color);

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

	generate_normals();
}

bool Cone::check_collision(const Maths::Ray& ray)
{
	// TODO
	return true;
}

Cube::Cube()
{
	vertices.reserve(6*6);

	auto add_face = [&](const glm::quat& rotator)
	{
		const int index_offset = vertices.size();
		Square front;
		// glm::mat4 idt(1.0f);
		// glm::translate(idt, )
		front.translate_vertices(glm::vec3(0.0f, 0.0f, 0.5f));
		front.transform_vertices(glm::mat4_cast(rotator));
		vertices.insert(vertices.end(),
						std::make_move_iterator(front.vertices.begin()),
						std::make_move_iterator(front.vertices.end()));
		for (auto index : front.indices)
		{
			indices.push_back(index + index_offset);
		}
	};

	add_face(glm::quat{});
	add_face(glm::angleAxis(-Maths::PI * 0.5f, glm::vec3(0.0f, 1.0f, 0.0f)));
	add_face(glm::angleAxis(-Maths::PI, glm::vec3(0.0f, 1.0f, 0.0f)));
	add_face(glm::angleAxis(-Maths::PI * 1.5f, glm::vec3(0.0f, 1.0f, 0.0f)));
	add_face(glm::angleAxis(-Maths::PI * 2.0f, glm::vec3(0.0f, 1.0f, 0.0f)));
	add_face(glm::angleAxis(-Maths::PI * 0.5f, glm::vec3(-1.0f, 0.0f, 0.0f)));
	add_face(glm::angleAxis(-Maths::PI * 0.5f, glm::vec3(1.0f, 0.0f, 0.0f)));

	// deduplicate_vertices(); // TODO: do this
	generate_normals();
}

bool Cube::check_collision(const Maths::Ray& ray)
{
	// TODO
	return true;
}

Tetrahedron::Tetrahedron()
{
	vertices.reserve(4);
	indices.reserve(12);

	const float a = std::sqrtf(4.0f/3.0f)/4.0f;
	const float b = std::sqrtf(8.0f/3.0f)/4.0f;
	const glm::vec3 color{1.0f, 1.0f, 1.0f};
	vertices.emplace_back(glm::vec3{-b, a, 0}, color);
	vertices.emplace_back(glm::vec3{b, a, 0}, color);
	vertices.emplace_back(glm::vec3{0, -a, -b}, color);
	vertices.emplace_back(glm::vec3{0, -a, b}, color);

	indices = {
		0, 1, 2,
		0, 2, 3,
		0, 3, 1,
		1, 3, 2
	};

	generate_normals();
}

bool Tetrahedron::check_collision(const Maths::Ray& ray)
{
	// TODO
	return true;
}

Icosahedron::Icosahedron()
{
	// https://schneide.blog/2016/07/15/generating-an-icosphere-in-c/
	const float X=.525731112119133606f;
	const float Z=.850650808352039932f;
	const float N=0.f;
	const glm::vec3 color{1.0f, 1.0f, 1.0f};
	vertices = {
		Vertex(glm::vec3{-X,N,Z}, color), Vertex(glm::vec3{X,N,Z}, color), 
		Vertex(glm::vec3{-X,N,-Z}, color), Vertex(glm::vec3{X,N,-Z}, color),
		Vertex(glm::vec3{N,Z,X}, color), Vertex(glm::vec3{N,Z,-X}, color), 
		Vertex(glm::vec3{N,-Z,X}, color), Vertex(glm::vec3{N,-Z,-X}, color),
		Vertex(glm::vec3{Z,X,N}, color), Vertex(glm::vec3{-Z,X, N}, color), 
		Vertex(glm::vec3{Z,-X,N}, color), Vertex(glm::vec3{-Z,-X, N}, color)
	};
	
	indices = {
		0,4,1,0,9,4,9,5,4,4,5,8,4,8,1,
		8,10,1,8,3,10,5,3,8,5,2,3,2,7,3,
		7,10,3,7,6,10,7,11,6,11,0,6,0,1,6,
		6,1,10,9,0,11,9,11,2,9,2,5,7,2,11
	};

	std::reverse(indices.begin(), indices.end());

	generate_normals();
}

bool Icosahedron::check_collision(const Maths::Ray& ray)
{
	// TODO
	return true;
}

}