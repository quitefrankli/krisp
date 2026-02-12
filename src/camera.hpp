#pragma once

#include "objects/object.hpp"
#include "objects/tracker.hpp"
#include "audio_engine/listener.hpp"

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/gtx/quaternion.hpp>

#include "maths.hpp"


class Camera : public Object, public ITrackableObject
{
public:
	Camera(Listener&& listener, float aspect_ratio);
	~Camera();

	glm::mat4 get_projection() const;
	glm::mat4 get_view() const;

	// converts a screen-space axis to a camera space axis
	glm::vec3 sync_to_camera(const glm::vec2& axis);

	// screen to world ray, pretty inefficient function atm,
	// the "screen" is essentially the mouse cursor location
	Maths::Ray get_ray(const glm::vec2& screen) const;

	glm::vec3 get_focus() const;

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

	void toggle_projection();

	void set_orthographic_projection(const glm::vec2& horizontal_span);

public: // object
	virtual void update_tracker() override;

	// this needs to be private and be manipulated within camera
	std::shared_ptr<Object> focus_obj; // might be better to give this object to game_engine

	// sets camera at "from" and the focus at "focus"
	void look_at(const glm::vec3& focus, const glm::vec3& from);
	// does not move camera but rotates it to look at "focus"
	void look_at(const glm::vec3& focus);
	// rotates camera based on screen offset from last screen pos
	void rotate_camera(const glm::vec2& offset, float delta_time);
	// pans both camera and focus in a direction
	void pan(const glm::vec3& relative_axis, const float magnitude);
	void pan(const glm::vec2& axis, const float magnitude);

	virtual void toggle_visibility() override;

protected:
	virtual void set_rotation(const glm::quat& rotation) override;

private:
	glm::mat4 perspective_matrix;
	glm::mat4 orthographic_matrix;

	Mode mode;
	Listener listener;

	using Object::set_transform;
	using Object::set_position;

	static constexpr float panning_sensitivity = 0.2f;
	bool projection_is_perspective = true;
	const float aspect_ratio;
	const float fov = Maths::deg2rad(45.0f);
	const float near_clipping = 0.1f;
	const float far_clipping = 250.0f;

	class CameraTests;
	friend CameraTests;
};