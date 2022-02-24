#include "shapes.hpp"

#include "maths.hpp"


namespace Shapes
{
Square::Square()
{
	// notice how the position has +Y going up and -Y going down
	// while the UV has +V going down and -V going up
	// this is because we are not flipping the textures when we load them
	// instead we are loading them in, in read order (as opposed to opengl)

	// our coordinate system is:
	// +X = right
	// +Y = up
	// +Z = out of screen
	// rotation is clockwise in the direction of the axis

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
}