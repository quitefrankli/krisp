#include "gizmo.hpp"

#include "objects/objects.hpp"
#include "graphics_engine/graphics_engine_commands.hpp"
#include "graphics_engine/graphics_engine.hpp"
#include "game_engine.hpp"
#include "shapes/shapes.hpp"
#include "camera.hpp"

#include <glm/gtx/string_cast.hpp>
#include <fmt/core.h>

#include <iostream>
#include <array>


//
// GizmoBase
//

GizmoBase::GizmoBase(GameEngine& engine_, Gizmo& gizmo_) :
	engine(engine_),
	gizmo(gizmo_)
{
}

bool GizmoBase::is_essential_child(Object* child)
{
	return std::any_of(axes.begin(), axes.end(), [child](auto* axis){ return child == axis;});
}

void GizmoBase::set_visibility(bool visibility)
{
	std::for_each(axes.begin(), axes.end(), [visibility](Object* axis)
	{
		axis->set_visibility(visibility);
	});
	Object::set_visibility(visibility);
}

//
// TranslationGizmo
//

void TranslationGizmo::init()
{
	xAxis.point(Maths::zero_vec, Maths::right_vec);
	yAxis.point(Maths::zero_vec, Maths::up_vec);
	zAxis.point(Maths::zero_vec, Maths::forward_vec);

	axes = {&xAxis, &yAxis, &zAxis};
	std::for_each(axes.begin(), axes.end(), [this](auto axis){
		axis->attach_to(this);
		engine.draw_object(*axis);
	});
	set_visibility(false);
}

bool TranslationGizmo::check_collision(const Maths::Ray& ray)
{
	if (!get_visibility())
	{
		return false;
	}

	// assume active_axis has been cleared already
	
	for (auto axis : axes)
	{
		if (axis->check_collision(ray))
		{
			active_axis = axis;
			reference_transform.position = get_position();
			reference_transform.orientation = get_rotation();

			const glm::vec3 curr_axis = active_axis->get_rotation() * Maths::forward_vec;
			plane.normal = glm::normalize(glm::cross(curr_axis, glm::cross(curr_axis, ray.direction)));;
			plane.offset = get_position();
			p1 = Maths::ray_plane_intersection(ray, plane);
			break;
		}
	}

	return active_axis;
}

void TranslationGizmo::process(const Maths::Ray& r1, const Maths::Ray& r2)
{
	if (!active_axis)
		return;

	const glm::vec3 curr_axis = active_axis->get_rotation() * Maths::forward_vec;

	const auto p2 = Maths::ray_plane_intersection(r2, plane);
	const auto Vp1_p2 = glm::dot(p2 - p1, curr_axis) * curr_axis;

	gizmo.set_position(reference_transform.position + Vp1_p2);
}

//
// RotationGizmo
//

void RotationGizmo::init()
{
	xAxisNorm.set_rotation(glm::angleAxis(-Maths::PI/2.0f, Maths::up_vec));
	yAxisNorm.set_rotation(glm::angleAxis(Maths::PI/2.0f, Maths::right_vec));

	axes = {&xAxisNorm, &yAxisNorm, &zAxisNorm};
	std::for_each(axes.begin(), axes.end(), [this](auto axis){
		axis->attach_to(this);
		engine.draw_object(*axis);
	});
	set_visibility(false);
}

bool RotationGizmo::check_collision(const Maths::Ray& ray)
{
	if (!get_visibility())
	{
		return false;
	}
	
	// assume active_axis has been cleared already
	
	for (auto axis : axes)
	{
		if (axis->check_collision(ray))
		{
			active_axis = axis;
			reference_transform.position = get_position();
			reference_transform.orientation = get_rotation();

			plane.normal = glm::normalize(active_axis->get_rotation() * Maths::forward_vec);
			plane.offset = get_position();
			p1 = Maths::ray_plane_intersection(ray, plane);
			break;
		}
	}

	return active_axis;
}

void RotationGizmo::process(const Maths::Ray& r1, const Maths::Ray& r2)
{
	if (!active_axis)
		return;

	const auto p2 = Maths::ray_plane_intersection(r2, plane);
	const auto quat = Maths::RotationBetweenVectors(
		glm::normalize(p1-plane.offset), 
		glm::normalize(p2-plane.offset),
		plane.normal);
	gizmo.set_rotation(glm::normalize(quat * reference_transform.orientation));
}

//
// ScaleGizmo
//

void ScaleGizmo::init()
{
	xAxis.point(Maths::zero_vec, Maths::right_vec);
	yAxis.point(Maths::zero_vec, Maths::up_vec);
	zAxis.point(Maths::zero_vec, Maths::forward_vec);

	axes = {&xAxis, &yAxis, &zAxis};
	std::for_each(axes.begin(), axes.end(), [this](auto axis){
		axis->attach_to(this);
		engine.draw_object(*axis);
	});
	set_visibility(false);
}

bool ScaleGizmo::check_collision(const Maths::Ray& ray)
{
	if (!get_visibility())
	{
		return false;
	}
	
	// assume active_axis has been cleared already
	
	for (auto axis : axes)
	{
		if (axis->check_collision(ray))
		{
			active_axis = axis;
			reference_transform.position = get_position();
			reference_transform.orientation = get_rotation();

			const glm::vec3 curr_axis = active_axis->get_rotation() * Maths::forward_vec;
			plane.normal = glm::normalize(glm::cross(curr_axis, glm::cross(curr_axis, ray.direction)));;
			plane.offset = get_position();
			p1 = Maths::ray_plane_intersection(ray, plane);
			break;
		}
	}

	return active_axis;
}

void ScaleGizmo::process(const Maths::Ray& r1, const Maths::Ray& r2)
{
	if (!active_axis)
		return;

	const glm::vec3 curr_axis = active_axis->get_rotation() * Maths::forward_vec;

	const auto p2 = Maths::ray_plane_intersection(r2, plane);
	const auto Vp1_p2 = glm::dot(p2 - p1, curr_axis) * curr_axis;

	gizmo.selected_object->set_scale(reference_transform.scale + Vp1_p2);
}

//
// Gizmo
//

Gizmo::Gizmo(GameEngine& engine_) : 
	engine(engine_),
	translation(engine_, *this),
	rotation(engine_, *this),
	scale(engine_, *this)
{
	translation.attach_to(this);
	rotation.attach_to(this);
	scale.attach_to(this);
}

void Gizmo::init()
{
	translation.init();
	rotation.init();
	scale.init();
}

void Gizmo::select_object(Object* obj)
{
	if (!obj)
		return;

	// if already currently selected object, change the gizmo mode
	if (obj == selected_object)
	{
		toggle_mode();
	}

	// gizmo can only be attached to 1 obj at a time
	fmt::print("Gizmo::select_object: obj_id={}\n", obj->get_id());
	deselect();
	selected_object = obj;
	set_position(selected_object->get_position());
	set_rotation(selected_object->get_rotation());
	selected_object->attach_to(this);

	isActive = true;

	if (scale_mode)
	{
		scale.set_visibility(true);
	} else
	{
		translation.set_visibility(true);
		rotation.set_visibility(true);
	}
}

void Gizmo::deselect()
{
	if (!selected_object)
		return;

	selected_object->detach_from();
	selected_object = nullptr;

	isActive = false;
	translation.set_visibility(false);
	rotation.set_visibility(false);
	scale.set_visibility(false);
}

void Gizmo::toggle_mode()
{
	scale_mode = !scale_mode;
}

void Gizmo::process(const Maths::Ray& r1, const Maths::Ray& r2)
{
	translation.process(r1, r2);
	rotation.process(r1, r2);
	scale.process(r1, r2);
}

bool Gizmo::check_collision(const Maths::Ray& ray)
{
	translation.clear_active_axis();
	rotation.clear_active_axis();
	scale.clear_active_axis();
	return translation.check_collision(ray) || rotation.check_collision(ray) || scale.check_collision(ray);
}

void Gizmo::delete_object()
{
	if (!selected_object)
		return;
		
	auto* obj = selected_object;
	deselect();
	engine.delete_object(obj->get_id());
}