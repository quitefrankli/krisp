#pragma once

#include "graphics_engine.hpp"

#include <glm/glm.hpp>

//
// for reference
//

// ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), // camera pos
// 					   glm::vec3(0.0f, 0.0f, 0.0f), // focus point
// 					   glm::vec3(0.0f, 0.0f, 1.0f)); // upvector, it gives view 'rotation' in most cases this would be actually up, however sometimes we may want it to look upside down
// ubo.proj = glm::perspective(3.1415f * 45.0f/180.0f, // 45deg fov
// 							(float)swap_chain_extent.width / (float)swap_chain_extent.height, // aspect ratio same as window
// 							0.1f, // near plane clipping, closest an object can be to camera
// 							10.0f); // far plane clipping, furthest away an object can be to camera	

class Camera
{
private:
	glm::vec3 position = glm::vec3(0.0f, 0.0f, -5.0f);
	glm::vec3 focus = glm::vec3(0.0f, 0.0f, 0.0f);
	glm::vec3 up_vector = glm::vec3(0.0f, 1.0f, 0.0f);
	glm::vec3 original_position = position; // for temporary shifting

	struct Perspective
	{
		float fov;
		float aspect_ratio;
		float near_clipping;
		float far_clipping;
	} perspective;

public:
	Camera(float aspect_ratio);
	~Camera() {};

	glm::mat4 get_perspective();
	glm::mat4 get_view();

	glm::vec3 get_position() { return position; }
	void set_position(glm::vec3 new_position);
	
	void rotate_by(glm::vec3 axis, float deg);

	void set_original_position(glm::vec3 pos) { original_position = pos; }

	void rotate_from_original_position(glm::vec3 axis, float deg);

	void reset_position() { position = original_position; }
};