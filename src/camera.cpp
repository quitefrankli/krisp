#include "camera.hpp"
#include "game_engine.hpp"
#include "maths.hpp"
#include "graphics_engine/graphics_engine.hpp"
#include "graphics_engine/graphics_engine_commands.hpp"

#include <glm/gtc/matrix_transform.hpp>


static glm::mat4 get_rotation_mat(glm::mat4 matrix)
{
	matrix[3] = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
	return matrix;
}

Camera::Camera(GameEngine& engine_, float aspect_ratio) :
	engine(engine_)
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
	// camera focus object
	
	focus_obj = std::make_shared<Cube>("../resources/textures/texture.jpg");
	focus_obj->set_scale(glm::vec3(0.3f, 0.3f, 0.3f));
	SpawnObjectCmd cmd;
	cmd.object = focus_obj;
	cmd.object_id = focus_obj->get_id();
	engine.get_graphics_engine().enqueue_cmd(std::make_unique<SpawnObjectCmd>(std::move(cmd)));
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

void Camera::set_transform(glm::mat4& transformation)
{
	Object::set_transform(transformation);
	up_vector = get_rotation() * ORIGINAL_UP_VECTOR;
	focus_obj->set_rotation(get_rotation());
}