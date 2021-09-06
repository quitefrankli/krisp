#include "camera.hpp"

#include "maths.hpp"

Camera::Camera(float aspect_ratio)
{
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
	return glm::lookAt(position, focus, up_vector);
}

void Camera::set_position(glm::vec3 new_position)
{
	position = new_position;
}

void Camera::rotate_by(glm::vec3 axis, float deg)
{
	glm::mat4 rotation_matrix = glm::rotate(glm::mat4(1), Maths::deg2rad(deg), axis);
	position = glm::vec3(rotation_matrix * glm::vec4(position, 1));
	std::cout << position.y << ' ' << position.z << std::endl;
}