#include "shapes.hpp"


glm::vec3 Shape::get_pos()
{
	return model[3];
}

void Shape::set_pos(glm::vec3 pos)
{
	model[3] = glm::vec4(pos, 1.0f);
	// TODO: shift the uniform buffer
}

Plane::Plane()
{
	vertices = 
	{
		{{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
		{{-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}}, 
		{{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
		{{0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
		{{-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
		{{0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}},
	};

	// for (int i = 0; i < 3; i++)
	// {
	// 	vertices[i].pos -= 0.3f;
	// }
}