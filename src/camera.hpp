#pragma once

#include "objects/object.hpp"
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/gtx/quaternion.hpp>

// for reference
// ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), // camera pos
// 					   glm::vec3(0.0f, 0.0f, 0.0f), // focus point
// 					   glm::vec3(0.0f, 0.0f, 1.0f)); // upvector, it gives view 'rotation' in most cases this would be actually up, however sometimes we may want it to look upside down
// ubo.proj = glm::perspective(3.1415f * 45.0f/180.0f, // 45deg fov
// 							(float)swap_chain_extent.width / (float)swap_chain_extent.height, // aspect ratio same as window
// 							0.1f, // near plane clipping, closest an object can be to camera
// 							10.0f); // far plane clipping, furthest away an object can be to camera	

class GameEngine;

class Camera : public Object
{
private:
	const glm::vec3 ORIGINAL_UP_VECTOR{ 0.f, 1.f, 0.f };
	glm::vec3 up_vector;
	glm::mat4 perspective_matrix;

public:
	glm::vec3 focus;
	glm::vec3 prev_focus; // TODO move this into a polymorphic tracker class

public:
	Camera(GameEngine& engine, float aspect_ratio);
	~Camera();

	glm::mat4 get_perspective();
	glm::mat4 get_view();

	// converts a screen-space axis to a camera space axis
	glm::vec3 sync_to_camera(const glm::vec2& axis);

public: // object
	// virtual glm::mat4 get_transform();
	// virtual glm::vec3 get_position() { return position; }
	// virtual glm::vec3 get_scale() { return scale; }
	// virtual glm::quat get_rotation() { return orientation; }

	virtual void set_transform(const glm::mat4& transform) override;
	// virtual void set_position(glm::vec3& position) override;
	// virtual void set_scale(glm::vec3& scale);
	// virtual void set_rotation(glm::quat& rotation) override;

	// this needs to be private and be manipulated within camera
	std::shared_ptr<Object> focus_obj; // might be better to give this object to game_engine
private:
	GameEngine& engine;
};