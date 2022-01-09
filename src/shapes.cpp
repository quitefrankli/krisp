#include "shapes.hpp"


void Shape::transform_vertices(const glm::mat4& transform)
{
	for (auto& vertex : vertices)
	{
		vertex.pos = glm::vec3(transform * glm::vec4(vertex.pos, 1));
		vertex.normal = glm::normalize(glm::vec3(transform * glm::vec4(vertex.normal, 1)));
	}
}

void Shape::generate_normals()
{
	// zero all normals
	std::for_each(vertices.begin(), vertices.end(), [](Vertex& vertex){vertex.normal = glm::vec3(0.0f);});
	
	// used for counting, after which we take the average of the normals
	std::vector<uint32_t> counter(indices.size(), 0);


	// every offset of 3 is a new triangle, we generate the normal by taking
	// the cross product of the sides
	for (int i = 0; i < indices.size(); i+=3)
	{
		glm::vec3 v1 = vertices[indices[i+1]].pos - vertices[indices[i]].pos;
		glm::vec3 v2 = vertices[indices[i+2]].pos - vertices[indices[i]].pos;
		glm::vec3 normal = glm::normalize(glm::cross(v1, v2));
		vertices[indices[i]].normal += normal;
		vertices[indices[i+1]].normal += normal;
		vertices[indices[i+2]].normal += normal;
		counter[indices[i]]++;
		counter[indices[i+1]]++;
		counter[indices[i+2]]++;
	}

	// 2nd pass we take the average
	for (const uint32_t index : indices)
	{
		vertices[index].normal = glm::normalize(vertices[index].normal / (float)counter[index]);
	}
}

Square::Square()
{
	vertices = 
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
		{{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}}, // bottom left
		{{ 0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}}, // bottom right
		{{ 0.5f,  0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}}, // top right
		{{-0.5f,  0.5f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}}, // top left
	};

	indices = 
	{
		0, 1, 2, 2, 3, 0
	};
}

Triangle::Triangle()
{
	throw std::runtime_error("not used");
	vertices = 
	{
		{{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
		{{0.0f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}}, 
		{{0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}}
	};
}