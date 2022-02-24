#pragma once

#include "objects/object.hpp"
#include "objects/tracker.hpp"

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/gtx/quaternion.hpp>

#include "maths.hpp"

// for reference
// ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), // camera pos
// 					   glm::vec3(0.0f, 0.0f, 0.0f), // focus point
// 					   glm::vec3(0.0f, 0.0f, 1.0f)); // upvector, it gives view 'rotation' in most cases this would be actually up, however sometimes we may want it to look upside down
// ubo.proj = glm::perspective(3.1415f * 45.0f/180.0f, // 45deg fov
// 							(float)swap_chain_extent.width / (float)swap_chain_extent.height, // aspect ratio same as window
// 							0.1f, // near plane clipping, closest an object can be to camera
// 							10.0f); // far plane clipping, furthest away an object can be to camera	

class GameEngine;

class Camera : public Object, public ITrackableObject
{
private:
	// not a constant vector, can change depending on camera
	glm::vec3 up_vector = Maths::up_vec;
	glm::mat4 perspective_matrix;
	glm::vec3 prev_focus;

public:
	Camera(GameEngine& engine, float aspect_ratio);
	~Camera();

	glm::mat4 get_perspective() const;
	glm::mat4 get_view() const;

	// converts a screen-space axis to a camera space axis
	glm::vec3 sync_to_camera(const glm::vec2& axis);

	// screen to world ray, pretty inefficient function atm,
	// the "screen" is essentially the mouse cursor location
	Maths::Ray get_ray(const glm::vec2& screen) const;

	glm::vec3 get_focus() const;
	glm::vec3 get_old_focus() const;

	void set_focal_length(float length);
	float get_focal_length();
	void zoom_in(float length);

	std::shared_ptr<Object> upvector_obj;
	
	enum class Mode
	{
		ORBIT, // similar to CAD
		FPV, // first person view like FPS
	};

	void toggle_mode();
	void set_mode(Mode mode);
	Mode get_mode() const { return mode; }


public: // object
	virtual void update_tracker() override;

	// this needs to be private and be manipulated within camera
	std::shared_ptr<Object> focus_obj; // might be better to give this object to game_engine

	// sets camera at "from" and the focus at "focus"
	void look_at(const glm::vec3& focus, const glm::vec3& from);
	// sets camera at "pos" but does not move focus
	void look_at(const glm::vec3& pos);
	// rotates camera based on screen offset from last screen pos
	void rotate_camera(const glm::vec2& offset, float delta_time);
	virtual void toggle_visibility() override;

protected:
	virtual void set_rotation(const glm::quat& rotation) override;

private:
	GameEngine& engine;
	Mode mode;

	using Object::set_transform;
	using Object::set_position;
};