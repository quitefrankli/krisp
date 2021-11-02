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
	up_vector = ORIGINAL_UP_VECTOR;
	set_position(glm::vec3(0.0f, 0.0f, 2.0f));

	const float fov = Maths::deg2rad(45.0f);
	// const float aspect_ratio = aspect_ratio; // passed in
	const float near_clipping = 0.1f;
	const float far_clipping = 100.0f;

	perspective_matrix = glm::perspective(fov, aspect_ratio, near_clipping, far_clipping);
	// ubo was originally designed for opengl whereby its y axis is flipped
	perspective_matrix[1][1] *= -1.0f; // NOTE removing with will cause issues with the culling
}

glm::vec3 Camera::sync_to_camera(const glm::vec2& axis)
{
	return glm::normalize(get_rotation() * glm::vec3(axis, 0.0f));
}

glm::mat4 Camera::get_perspective()
{
	return perspective_matrix;
}

glm::mat4 Camera::get_view()
{
	return glm::lookAt(get_position(), focus, up_vector);
}

void Camera::set_transformation(glm::mat4 transformation)
{
	Object::set_transform(transformation);
	up_vector = get_rotation() * ORIGINAL_UP_VECTOR;
}