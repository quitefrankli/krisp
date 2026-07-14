#include "camera.hpp"
#include "game_engine.hpp"
#include "maths.hpp"
#include "objects/objects.hpp"
#include "audio_engine/listener.hpp"
#include "renderable/mesh_factory.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/string_cast.hpp>

#include <iostream>


Camera::Camera(Listener&& listener, float aspect_ratio) :
	ITrackableObject(this),
	listener(std::move(listener)),
	aspect_ratio(aspect_ratio)
{
	perspective_matrix = glm::perspectiveLH(fov, aspect_ratio, near_clipping, far_clipping);
	orthographic_matrix = glm::orthoLH(-aspect_ratio, aspect_ratio, -1.0f, 1.0f, near_clipping, far_clipping);
	focus_obj = std::make_shared<Object>(Renderable::make_default(MeshFactory::sphere_id()));
	focus_obj->set_scale(glm::vec3(0.3f, 0.3f, 0.3f));
	focus_obj->set_visibility(false);

	upvector_obj = std::make_shared<Arrow>();
	upvector_obj->set_position(get_focus());
	upvector_obj->set_visibility(false);

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
	auto proj_mat = get_projection();
	const auto proj_view_mat = glm::inverse(proj_mat * get_view());

	auto unproj = [&](const float depth)
	{
		auto point = proj_view_mat * glm::vec4(screen, depth, 1.0f);
		point /= point.w;
		return glm::vec3(point);
	};

	// GLM is configured for Vulkan's [0, 1] clip-space depth range.  Unproject
	// the actual near and far clip planes so this ray exactly matches the camera
	// projection rather than relying on sampled depth values.
	const auto near_point = unproj(0.0f);
	const auto far_point = unproj(1.0f);
	const auto direction = glm::normalize(far_point - near_point);

	// Perspective rays all pass through the camera position. Orthographic rays
	// are parallel but begin at a different point on the near plane for each
	// screen coordinate.
	return Maths::Ray(projection_is_perspective ? get_position() : near_point, direction);
}

glm::mat4 Camera::get_projection() const
{
	return projection_is_perspective ? perspective_matrix : orthographic_matrix;
}

glm::mat4 Camera::get_view() const
{
	return glm::lookAtLH(get_position(), get_focus(), focus_obj->get_rotation() * Maths::up_vec);
}

void Camera::set_rotation(const glm::quat& rotation)
{
	Object::set_rotation(rotation);
}

void Camera::toggle_projection()
{
	projection_is_perspective = !projection_is_perspective;
}

void Camera::set_orthographic_projection(const glm::vec2& horizontal_span)
{
	orthographic_matrix = glm::orthoLH(
		horizontal_span.x, 
		horizontal_span.y, 
		horizontal_span.x / aspect_ratio, 
		horizontal_span.y / aspect_ratio, 
		near_clipping, 
		far_clipping);
}

void Camera::update_tracker()
{
	set_old_position(focus_obj->get_position());
	set_old_rotation(focus_obj->get_rotation());
	set_old_scale(get_scale());
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

void Camera::look_at(const glm::vec3& focus)
{
	look_at(focus, get_position());
}

void Camera::pan(const glm::vec3& relative_axis, const float magnitude)
{
	const glm::vec3 offset = relative_axis * magnitude;
	// The camera is attached to focus_obj in orbit mode, so moving the focus
	// already moves the camera. Preserve its pre-pan world position to avoid
	// applying the translation twice and changing the camera-focus direction.
	const glm::vec3 camera_position = get_position();
	focus_obj->set_position(get_focus() + offset);
	Object::set_position(camera_position + offset);
}

void Camera::pan(const glm::vec2& axis, const float magnitude)
{
	// for 2d sensitivity is also a function of the focal length
	const float sensitivity = panning_sensitivity * get_focal_length();
	const glm::vec3 vec = sync_to_camera(axis);
	pan(vec, magnitude * sensitivity);
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

	if (!projection_is_perspective)
	{
		length = Maths::absf(length);
		orthographic_matrix = glm::orthoLH(
			-aspect_ratio * length, aspect_ratio * length, -length, length, near_clipping, far_clipping);
	}
}

void Camera::toggle_visibility()
{
	focus_obj->toggle_visibility();
	upvector_obj->toggle_visibility();
	Object::toggle_visibility();
}

void Camera::rotate_camera(const glm::vec2& offset, float delta_time)
{
	const float sensitivity = 200.0f;
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
