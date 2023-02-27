#pragma once

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

template<typename GameEngineT>
GizmoBase<GameEngineT>::GizmoBase(GameEngineT& engine, Gizmo<GameEngineT>& gizmo_) :
	engine(engine),
	gizmo(gizmo_)
{
}

template<typename GameEngineT>
bool GizmoBase<GameEngineT>::is_essential_child(Object* child)
{
	return std::any_of(axes.begin(), axes.end(), [child](auto* axis){ return child == axis;});
}

template<typename GameEngineT>
void GizmoBase<GameEngineT>::set_visibility(bool visibility)
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

template<typename GameEngineT>
void TranslationGizmo<GameEngineT>::init()
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

template<typename GameEngineT>
bool TranslationGizmo<GameEngineT>::check_collision(const Maths::Ray& ray)
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
			reference_transform.set_pos(get_position());
			reference_transform.set_orient(get_rotation());

			const glm::vec3 curr_axis = active_axis->get_rotation() * Maths::forward_vec;
			plane.normal = glm::normalize(glm::cross(curr_axis, glm::cross(curr_axis, ray.direction)));;
			plane.offset = get_position();
			p1 = Maths::ray_plane_intersection(ray, plane);
			break;
		}
	}

	return active_axis;
}

template<typename GameEngineT>
void TranslationGizmo<GameEngineT>::process(const Maths::Ray& r1, const Maths::Ray& r2)
{
	if (!active_axis)
		return;

	const glm::vec3 curr_axis = active_axis->get_rotation() * Maths::forward_vec;

	const auto p2 = Maths::ray_plane_intersection(r2, plane);
	const auto Vp1_p2 = glm::dot(p2 - p1, curr_axis) * curr_axis;

	gizmo.set_position(reference_transform.get_pos() + Vp1_p2);
}

//
// RotationGizmo
//

template<typename GameEngineT>
void RotationGizmo<GameEngineT>::init()
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

template<typename GameEngineT>
bool RotationGizmo<GameEngineT>::check_collision(const Maths::Ray& ray)
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
			reference_transform.set_pos(get_position());
			reference_transform.set_orient(get_rotation());

			plane.normal = glm::normalize(active_axis->get_rotation() * Maths::forward_vec);
			plane.offset = get_position();
			p1 = Maths::ray_plane_intersection(ray, plane);
			break;
		}
	}

	return active_axis;
}

template<typename GameEngineT>
void RotationGizmo<GameEngineT>::process(const Maths::Ray& r1, const Maths::Ray& r2)
{
	if (!active_axis)
		return;

	const auto p2 = Maths::ray_plane_intersection(r2, plane);
	const auto quat = Maths::RotationBetweenVectors(
		glm::normalize(p1-plane.offset), 
		glm::normalize(p2-plane.offset),
		plane.normal);
	gizmo.set_rotation(glm::normalize(quat * reference_transform.get_orient()));
}

//
// ScaleGizmo
//

template<typename GameEngineT>
ScaleGizmo<GameEngineT>::ScaleGizmo(GameEngineT& engine, Gizmo<GameEngineT>& gizmo) :
	xAxis(Maths::right_vec),
	yAxis(Maths::up_vec),
	zAxis(Maths::forward_vec),
	GizmoBase<GameEngineT>(engine, gizmo)
{
}

template<typename GameEngineT>
void ScaleGizmo<GameEngineT>::init()
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

template<typename GameEngineT>
bool ScaleGizmo<GameEngineT>::check_collision(const Maths::Ray& ray)
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
			reference_transform.set_scale(gizmo.selected_object->get_scale());

			const glm::vec3 curr_axis = active_axis->get_rotation() * Maths::forward_vec;
			plane.normal = glm::normalize(glm::cross(curr_axis, glm::cross(curr_axis, ray.direction)));;
			plane.offset = get_position();
			p1 = Maths::ray_plane_intersection(ray, plane);
			break;
		}
	}

	return active_axis;
}

template<typename GameEngineT>
void ScaleGizmo<GameEngineT>::process(const Maths::Ray& r1, const Maths::Ray& r2)
{
	if (!active_axis)
		return;

	const glm::vec3 curr_axis = active_axis->get_rotation() * Maths::forward_vec;

	const auto p2 = Maths::ray_plane_intersection(r2, plane);
	const float magnitude = glm::dot(p2 - p1, curr_axis);
	// const auto Vp1_p2 = tmp * curr_axis;

	const auto& original_axis = static_cast<ScaleGizmoObj*>(active_axis)->original_axis;
	const glm::vec3 new_scale = reference_transform.get_scale() + original_axis * magnitude;
	
	if (glm::dot(new_scale, original_axis) < minimum_scale)
	{
		return;
	}

	gizmo.selected_object->set_scale(new_scale);
}

//
// Gizmo
//

template<typename GameEngineT>
Gizmo<GameEngineT>::Gizmo(GameEngineT& engine) : 
	engine(engine),
	translation(engine, *this),
	rotation(engine, *this),
	scale(engine, *this)
{
	translation.attach_to(this);
	rotation.attach_to(this);
	scale.attach_to(this);
}

template<typename GameEngineT>
void Gizmo<GameEngineT>::init()
{
	translation.init();
	rotation.init();
	scale.init();
}

template<typename GameEngineT>
void Gizmo<GameEngineT>::select_object(Object* obj)
{
	if (!obj)
		return;

	// if already currently selected object, change the gizmo mode
	if (obj == selected_object)
	{
		toggle_mode();
	}

	// gizmo can only be attached to 1 obj at a time
	deselect();
	engine.highlight_object(*obj);
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

template<typename GameEngineT>
void Gizmo<GameEngineT>::deselect()
{
	if (!selected_object)
		return;

	engine.unhighlight_object(*selected_object);
	selected_object->detach_from();
	selected_object = nullptr;

	isActive = false;
	translation.set_visibility(false);
	rotation.set_visibility(false);
	scale.set_visibility(false);
}

template<typename GameEngineT>
void Gizmo<GameEngineT>::toggle_mode()
{
	scale_mode = !scale_mode;
}

template<typename GameEngineT>
void Gizmo<GameEngineT>::process(const Maths::Ray& r1, const Maths::Ray& r2)
{
	translation.process(r1, r2);
	rotation.process(r1, r2);
	scale.process(r1, r2);
}

template<typename GameEngineT>
bool Gizmo<GameEngineT>::check_collision(const Maths::Ray& ray)
{
	translation.clear_active_axis();
	rotation.clear_active_axis();
	scale.clear_active_axis();
	return translation.check_collision(ray) || rotation.check_collision(ray) || scale.check_collision(ray);
}

template<typename GameEngineT>
void Gizmo<GameEngineT>::delete_object()
{
	if (!selected_object)
		return;
		
	auto* obj = selected_object;
	deselect();
	engine.delete_object(obj->get_id());
}