#include "camera.hpp"

#include "maths.hpp"

#include <glm/gtc/matrix_transform.hpp>


static glm::mat4 get_rotation_mat(glm::mat4 matrix)
{
	matrix[3] = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
	return matrix;
}

Camera::Camera(float aspect_ratio)
{
	focus = glm::vec3(0.0f);
	original_transformation = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 2.0f));
	original_up_vector = glm::vec3(0.0f, 1.0f, 0.0f);
	up_vector = original_up_vector;
	assert(transformation == glm::mat4(1.0f));
	quat = glm::quat_cast(transformation);

	transformation = original_transformation;

	perspective.fov = Maths::deg2rad(45.0f);
	perspective.aspect_ratio = aspect_ratio;
	perspective.near_clipping = 0.1f;
	perspective.far_clipping = 100.0f;

}

glm::vec3 Camera::sync_to_camera(const glm::vec2& axis)
{
	return glm::normalize(quat * glm::vec3(axis, 0.0f));
}

glm::mat4 Camera::get_perspective()
{
	auto proj = glm::perspective(perspective.fov, perspective.aspect_ratio, perspective.near_clipping, perspective.far_clipping);
	// ubo was originally designed for opengl whereby its y axis is flipped
	proj[1][1] *= -1.0f; // NOTE removing with will cause issues with the culling
	return proj;
}

glm::mat4 Camera::get_view()
{
	return glm::lookAt(get_position(), focus, up_vector);
}

void Camera::set_transformation(const glm::mat4& transformation)
{
	ObjectAbstract::set_transformation(transformation);
	up_vector = transformation * glm::vec4(original_up_vector, 1);
	quat = glm::normalize(glm::quat_cast(glm::mat3(transformation)));
}

void Camera::apply_transformation(const glm::mat4& transformation)
{
	// ObjectAbstract::apply_transformation(transformation);
	this->transformation = transformation * original_transformation;
	// this->transformation = original_transformation * transformation;

	up_vector = transformation * glm::vec4(original_up_vector, 1);
}