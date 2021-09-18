#include "objects.hpp"
#include "maths.hpp"

#include <glm/gtc/matrix_transform.hpp>

Pyramid::Pyramid()
{
	Triangle left, right, back, bottom;
	{
		glm::mat4 transform(1.0f);
		transform = glm::rotate(transform, Maths::deg2rad(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
		transform = glm::translate(transform, glm::vec3(-0.5f, 0.0f, 0.0f));
		left.transform_vertices(transform);
	}
	{
		glm::mat4 transform(1.0f);
		transform = glm::rotate(transform, Maths::deg2rad(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
		transform = glm::translate(transform, glm::vec3(0.5f, 0.0f, 0.0f));
		left.transform_vertices(transform);
	}
	{
		glm::mat4 transform(1.0f);
		// transform = glm::rotate(transform, Maths::deg2rad(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
		transform = glm::translate(transform, glm::vec3(0.0f, 0.0f, 0.3f));
		left.transform_vertices(transform);
	}
	{
		glm::mat4 transform(1.0f);
		transform = glm::rotate(transform, Maths::deg2rad(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
		transform = glm::translate(transform, glm::vec3(0.0f, -0.5f, 0.0f));
		left.transform_vertices(transform);
	}
	shapes = std::vector<Shape>{ left, right, back, bottom };
}