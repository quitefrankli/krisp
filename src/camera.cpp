#include "camera.hpp"
#include "game_engine.hpp"
#include "maths.hpp"
#include "objects/objects.hpp"
#include "audio_engine/listener.hpp"
#include "input.hpp"
#include "renderable/mesh_factory.hpp"
#include "serialization/serialization_helpers.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/string_cast.hpp>

#include <utility>

namespace
{
struct CameraBasis
{
	glm::vec3 right;
	glm::vec3 up;
	glm::vec3 forward;
};

CameraBasis make_roll_free_basis(const glm::vec3& direction)
{
	const glm::vec3 forward = glm::normalize(direction);
	glm::vec3 right = glm::cross(Maths::up_vec, forward);
	if (glm::length2(right) <= Maths::ACCEPTABLE_FLOATING_PT_DIFF)
		right = Maths::right_vec;
	else
		right = glm::normalize(right);

	return {right, glm::normalize(glm::cross(forward, right)), forward};
}

glm::quat make_roll_free_orientation(const glm::vec3& direction)
{
	const auto basis = make_roll_free_basis(direction);
	return glm::normalize(glm::quat_cast(glm::mat3(basis.right, basis.up, basis.forward)));
}
}

void Camera::serialize(Serializer& out) const
{
	Serialization::write_vec3(out, "position", get_position());
	Serialization::write_vec3(out, "focus", get_focus());
	out.write("mode", static_cast<int>(mode));
	out.write("perspective", projection_is_perspective);
	Serialization::write_vec2(out, "orthographic_horizontal_span", orthographic_horizontal_span);
}

void Camera::deserialize(const Deserializer& in)
{
	look_at(
		Serialization::read_vec3(in, "focus"),
		Serialization::read_vec3(in, "position"));
	set_orthographic_projection(Serialization::read_vec2(in, "orthographic_horizontal_span"));
	if (projection_is_perspective != in.read<bool>("perspective"))
		toggle_projection();
	set_mode(static_cast<Mode>(in.read<int>("mode")));
}


Camera::Camera(ECS& ecs, Listener&& listener, float aspect_ratio, Object& focus, Object& upvector) :
	listener(std::move(listener)),
	aspect_ratio(aspect_ratio),
	ecs(ecs)
{
	ecs.add_transformation(get_id(), TransformationPersistence::Transient);
	perspective_matrix = glm::perspectiveLH(fov, aspect_ratio, near_clipping, far_clipping);
	set_orthographic_projection({ -aspect_ratio, aspect_ratio });
	focus_obj = &focus;
	upvector_obj = &upvector;
	ecs.get_transformation(focus_obj->get_id()).set_scale(glm::vec3(0.3f));
	focus_obj->set_visibility(false);
	ecs.get_transformation(upvector_obj->get_id()).set_position(get_focus());
	upvector_obj->set_visibility(false);
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

Camera::~Camera()
{
	ecs.remove_transformation(get_id());
}

TransformationComponent& Camera::transformation()
{
	return ecs.get_transformation(get_id());
}

const TransformationComponent& Camera::transformation() const
{
	return std::as_const(ecs).get_transformation(get_id());
}

glm::vec3 Camera::sync_to_camera(const glm::vec2& axis)
{
	const auto basis = make_roll_free_basis(get_focus() - get_position());
	return glm::normalize(basis.right * axis.x + basis.up * axis.y);
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
	const auto basis = make_roll_free_basis(get_focus() - get_position());
	return glm::lookAtLH(get_position(), get_focus(), basis.up);
}

void Camera::set_rotation(const glm::quat& rotation)
{
	transformation().set_rotation(make_roll_free_orientation(rotation * Maths::forward_vec));
}

void Camera::set_position(const glm::vec3& position)
{
	transformation().set_position(position);
}

glm::vec3 Camera::get_position() const
{
	return transformation().get_position();
}

glm::vec3 Camera::get_scale() const
{
	return transformation().get_scale();
}

glm::quat Camera::get_rotation() const
{
	return transformation().get_rotation();
}

void Camera::toggle_projection()
{
	projection_is_perspective = !projection_is_perspective;
}

void Camera::set_orthographic_projection(const glm::vec2& horizontal_span)
{
	orthographic_horizontal_span = horizontal_span;
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
	const auto& focus_transform = ecs.get_transformation(focus_obj->get_id());
	set_old_position(focus_transform.get_position());
	set_old_rotation(focus_transform.get_rotation());
	set_old_scale(get_scale());
}

glm::vec3 Camera::get_focus() const
{
	return ecs.get_transformation(focus_obj->get_id()).get_position();
}

void Camera::sync_audio_listener()
{
	const auto basis = make_roll_free_basis(get_focus() - get_position());
	listener.set_transform(get_position(), basis.forward, basis.up);
}

void Camera::look_at(const glm::vec3& focus, const glm::vec3& from)
{
	const glm::vec3 view_dir = glm::normalize(focus - from);
	auto& focus_transform = ecs.get_transformation(focus_obj->get_id());
	focus_transform.set_position(focus);
	focus_transform.set_rotation(make_roll_free_orientation(view_dir));
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
	ecs.get_transformation(focus_obj->get_id()).set_position(get_focus() + offset);
	transformation().set_position(camera_position + offset);
}

void Camera::process_keyboard_movement(const Keyboard& keyboard, const float delta_secs)
{
	const float move_speed = 1.5f * get_focal_length();
	glm::vec3 offset = Maths::zero_vec;
	const glm::vec3 up = get_rotation() * Maths::up_vec;
	const glm::vec3 right = get_rotation() * Maths::right_vec;
	if (keyboard.w_pressed())
		offset += up * move_speed * delta_secs;
	if (keyboard.s_pressed())
		offset -= up * move_speed * delta_secs;
	if (keyboard.a_pressed())
		offset -= right * move_speed * delta_secs;
	if (keyboard.d_pressed())
		offset += right * move_speed * delta_secs;
	if (offset != Maths::zero_vec)
		pan(offset, 1.0f);
}

void Camera::follow(Object& target, const glm::vec3& focus_offset, const float horizontal_focus_offset)
{
	ecs.get_transformation(focus_obj->get_id()).detach_from();
	follow_target = &target;
	follow_offset = focus_offset;
	follow_horizontal_offset = horizontal_focus_offset;
	update_follow();
}

void Camera::stop_follow()
{
	follow_target = nullptr;
}

void Camera::update_follow()
{
	if (follow_target)
	{
		const auto basis = make_roll_free_basis(get_focus() - get_position());
		const glm::vec3 desired_focus = ecs.get_transformation(follow_target->get_id()).get_position()
			+ follow_offset + basis.right * follow_horizontal_offset;
		pan(desired_focus - get_focus(), 1.0f);
	}
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
	const glm::vec3 offset =
		ecs.get_transformation(focus_obj->get_id()).get_rotation() * Maths::forward_vec * length;
	set_position(get_position() + offset);

	if (!projection_is_perspective)
	{
		length = Maths::absf(length);
		orthographic_matrix = glm::orthoLH(
			-aspect_ratio * length, aspect_ratio * length, -length, length, near_clipping, far_clipping);
		orthographic_horizontal_span = { -aspect_ratio * length, aspect_ratio * length };
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
	const float rotation_scale = delta_time * sensitivity;
	const glm::vec3 direction = glm::normalize(get_focus() - get_position());
	const auto basis = make_roll_free_basis(direction);

	const float yaw = -offset.x * rotation_scale;
	const glm::vec3 horizontal_forward = glm::normalize(glm::cross(basis.right, Maths::up_vec));
	const glm::vec3 yawed_horizontal = glm::angleAxis(yaw, Maths::up_vec) * horizontal_forward;

	constexpr float MAX_PITCH = glm::radians(89.0f);
	const float current_pitch = std::asin(glm::clamp(glm::dot(direction, Maths::up_vec), -1.0f, 1.0f));
	const float pitch = glm::clamp(current_pitch - offset.y * rotation_scale, -MAX_PITCH, MAX_PITCH);
	const glm::vec3 new_direction = glm::normalize(
		yawed_horizontal * std::cos(pitch) + Maths::up_vec * std::sin(pitch));
	const glm::quat orientation = make_roll_free_orientation(new_direction);

	switch (mode)
	{
		case Mode::ORBIT:
		{
			ecs.get_transformation(focus_obj->get_id()).set_rotation(orientation);
			break;
		}
		case Mode::FPV:
		{
			set_rotation(orientation);
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
		auto& focus_transform = ecs.get_transformation(focus_obj->get_id());
		focus_transform.detach_from();
		ecs.get_transformation(upvector_obj->get_id()).attach_to(focus_transform);
		transformation().attach_to(focus_transform);
	} else if (new_mode == Mode::FPV) {
		auto& camera_transform = transformation();
		camera_transform.detach_from();
		ecs.get_transformation(focus_obj->get_id()).attach_to(camera_transform);
	} else {
		assert(false);
	}

	mode = new_mode;
}
