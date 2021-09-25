#include "camera.hpp"

#include "maths.hpp"

#include <glm/gtc/matrix_transform.hpp>

Camera::Camera(float aspect_ratio)
{
	original_transformation = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 2.0f));
	transformation = original_transformation;

	perspective.fov = Maths::deg2rad(45.0f);
	perspective.aspect_ratio = aspect_ratio;
	perspective.near_clipping = 0.1f;
	perspective.far_clipping = 10.0f;
}

glm::mat4 Camera::get_perspective()
{
	return glm::perspective(perspective.fov, perspective.aspect_ratio, perspective.near_clipping, perspective.far_clipping);
}

glm::mat4 Camera::get_view()
{
	return glm::lookAt(get_position(), focus, up_vector);
}

void Camera::reset_transformation()
{
	// ObjectAbstract::set_transformation(original_transformation);
	// up_vector = original_up_vector;
	ObjectAbstract::set_transformation(glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -5.0f)));
	up_vector = glm::vec3(0.0f, 1.0f, 0.0f);
}

void Camera::set_transformation(const glm::mat4& transformation)
{
	ObjectAbstract::set_transformation(transformation);
	up_vector = transformation * glm::vec4(original_up_vector, 1);
}

void Camera::apply_transformation(const glm::mat4& transformation)
{
	// ObjectAbstract::apply_transformation(transformation);
	this->transformation = transformation * original_transformation;
	// this->transformation = original_transformation * transformation;

	up_vector = transformation * glm::vec4(original_up_vector, 1);
}