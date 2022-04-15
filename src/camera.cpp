#include "camera.hpp"
#include "game_engine.hpp"
#include "maths.hpp"
#include "objects/objects.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/string_cast.hpp>

#include <iostream>


Camera::Camera(GameEngine& engine_, float aspect_ratio) :
	ITrackableObject(this),
	engine(engine_)
{
	set_position(glm::vec3(0.0f, 0.0f, -2.0f));

	const float fov = Maths::deg2rad(45.0f);
	// const float aspect_ratio = aspect_ratio; // passed in
	const float near_clipping = 0.1f;
	const float far_clipping = 250.0f;

	perspective_matrix = glm::perspectiveLH(fov, aspect_ratio, near_clipping, far_clipping);
	focus_obj = std::make_shared<Sphere>();
	focus_obj->set_scale(glm::vec3(0.3f, 0.3f, 0.3f));
	focus_obj->set_visibility(false);
	engine.draw_object(focus_obj);

	upvector_obj = std::make_shared<Arrow>();
	upvector_obj->set_position(get_focus());
	upvector_obj->set_visibility(false);
	engine.draw_object(upvector_obj);

	attach_to(focus_obj.get());
	upvector_obj->attach_to(focus_obj.get());

	set_mode(Mode::ORBIT);
	set_visibility(false);

// without GLM_FORCE_DEPTH_ZERO_TO_ONE
	// my proj
	//  1.37,  0.00,  0.00,  0.00,
	//  0.00,  2.41,  0.00,  0.00,
	//  0.00,  0.00,  1.00,  1.00,
	//  0.00,  0.00, -0.20,  0.00,
	// my view
	// -1.00,  0.00,  0.00,  0.00,
	//  0.00,  1.00,  0.00,  0.00,
	//  0.00,  0.00, -1.00,  0.00,
	//  0.00,  0.00, -2.00,  1.00,


// with GLM_FORCE_DEPTH_ZERO_TO_ONE
	// my proj
	// 1.37,  0.00,  0.00,  0.00,
	// 0.00,  2.41,  0.00,  0.00,
	// 0.00,  0.00,  1.00,  1.00,
	// 0.00,  0.00, -0.10,  0.00,
	// my view
	// -1.00,  0.00,  0.00,  0.00,
	// 0.00,  1.00,  0.00,  0.00,
	// 0.00,  0.00, -1.00,  0.00,
	// 0.00,  0.00, -2.00,  1.00,
}

Camera::~Camera() = default;

glm::vec3 Camera::sync_to_camera(const glm::vec2& axis)
{
	return glm::normalize(focus_obj->get_rotation() * glm::vec3(axis, 0.0f));
}

Maths::Ray Camera::get_ray(const glm::vec2& screen) const
{
	auto proj_mat = get_perspective();
	const auto proj_view_mat = glm::inverse(proj_mat * get_view());

	auto unproj = [&](const float depth)
	{
		auto point = proj_view_mat * glm::vec4(screen, depth, 1.0f);
		point /= point.w;
		return glm::vec3(point);
	};

	// arbitrary constants that seem to work, although i have no idea what they mean, just got them by experimentation
	// it's supposedly for the near and far planes but it doesn't appear to be linear
	const auto p1 = unproj(0.7f);
	const auto p2 = unproj(0.95f);

	return Maths::Ray(get_position(), glm::normalize(p2-p1));
}

glm::mat4 Camera::get_perspective() const
{
	return perspective_matrix;
}

glm::mat4 Camera::get_view() const
{
	return glm::lookAtLH(get_position(), get_focus(), focus_obj->get_rotation() * Maths::up_vec);
}

void Camera::set_rotation(const glm::quat& rotation)
{
	Object::set_rotation(rotation);
}

void Camera::update_tracker()
{
	set_old_position(focus_obj->get_position());
	set_old_rotation(focus_obj->get_rotation());
	set_old_scale(get_scale());
	prev_focus = get_focus();
}

glm::vec3 Camera::get_focus() const
{
	return focus_obj->get_position();
}

void Camera::look_at(const glm::vec3& focus, const glm::vec3& from)
{
	const glm::vec3 view_dir = glm::normalize(focus - from);
	focus_obj->set_position(focus);
	focus_obj->set_rotation(Maths::RotationBetweenVectors(Maths::forward_vec, view_dir));
	set_position(from);
}

void Camera::look_at(const glm::vec3& pos)
{
	look_at(pos, get_position());
}

void Camera::pan(const glm::vec3& axis, const float magnitude)
{
	focus_obj->set_position(get_old_focus() + axis * magnitude);
}

void Camera::pan(const glm::vec2& axis, const float magnitude)
{
	const float sensitivity = 1.0f;
	const glm::vec3 vec = sync_to_camera(axis);
	pan(vec, magnitude * sensitivity);
}

glm::vec3 Camera::get_old_focus() const
{
	return prev_focus;
}

float Camera::get_focal_length()
{
	return glm::distance(get_focus(), get_position());
}

void Camera::zoom_in(float length)
{
	// +ve length zooms in, -ve length zooms out
	const float closest_distance = 1.0f;
	const float maximum_distance = 100.0f;
	
	const float focal_len = get_focal_length();

	// apply zoom relative to current length, this way zoom covers large distane when already far away
	const float sensitivity = 0.2f * focal_len;
	length *= sensitivity;
	length = std::min(focal_len - closest_distance, length);
	length = std::max(focal_len - maximum_distance, length);
	const glm::vec3 offset = focus_obj->get_rotation() * Maths::forward_vec * length;
	set_position(get_position() + offset);
}

void Camera::toggle_visibility()
{
	focus_obj->toggle_visibility();
	upvector_obj->toggle_visibility();
	Object::toggle_visibility();
}

void Camera::rotate_camera(const glm::vec2& offset, float delta_time)
{
	const float sensitivity = 0.2f;
	const float magnitude = glm::length(offset) * delta_time * sensitivity;

	// I did some mental maths and actually this checks out
	// using left hand with thumb in direction of axis and fingers curling
	// in direction of curling, one of the dimensions needs to have its sign flipped
	// seems there are no bugs here
	const glm::vec2 screen_axis(-offset.y, offset.x);
	const glm::vec3 axis = sync_to_camera(screen_axis);
	const glm::quat quaternion = glm::angleAxis(-magnitude, axis);

	switch (mode)
	{
		case Mode::ORBIT:
		{
			focus_obj->set_rotation(quaternion * focus_obj->get_rotation());
			break;
		}
		case Mode::FPV:
		{
			set_rotation(quaternion * get_rotation());
			break;
		}
		default:
			assert(false);
	}
}

void Camera::toggle_mode()
{
	set_mode(mode == Mode::FPV ? Mode::ORBIT : Mode::FPV);
}

void Camera::set_mode(Mode new_mode)
{
	if (new_mode == Mode::ORBIT)
	{
		focus_obj->detach_from();
		upvector_obj->attach_to(focus_obj.get()); // don't forget to reattach it
		attach_to(focus_obj.get());
	} else if (new_mode == Mode::FPV) {
		detach_from();
		focus_obj->attach_to(this);
	} else {
		assert(false);
	}

	mode = new_mode;
}