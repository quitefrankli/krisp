#pragma once

#include "objects.hpp"

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

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

class Camera : public ObjectAbstract // in future we should we swap this for non-abstract Object
{
private:
	glm::vec3 focus = glm::vec3(0.0f, 0.0f, 0.0f);
	glm::vec3 original_up_vector = glm::vec3(0.0f, 1.0f, 0.0f);
	glm::vec3 up_vector = original_up_vector;

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

	void reset_transformation();

	void set_transformation(const glm::mat4& transformation) override;

	void apply_transformation(const glm::mat4& transformation) override;
};