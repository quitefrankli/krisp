#pragma once

#include "gizmo.hpp"

#include "renderable/render_types.hpp"
#include "objects/objects.hpp"
#include "game_engine.hpp"
#include "camera.hpp"
#include "renderable/material_factory.hpp"
#include "renderable/mesh_factory.hpp"
#include "renderable/mesh_maths.hpp"
#include "entity_component_system/mesh_system.hpp"
#include "collision/collider.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtx/component_wise.hpp>
#include <fmt/core.h>

#include <iostream>
#include <array>

namespace
{
constexpr std::array axis_materials = {
	EMaterialPreset::GIZMO_X_AXIS,
	EMaterialPreset::GIZMO_Y_AXIS,
	EMaterialPreset::GIZMO_Z_AXIS,
};

constexpr std::array rotation_axis_materials = {
	EMaterialPreset::GIZMO_Z_AXIS,
	EMaterialPreset::GIZMO_Y_AXIS,
	EMaterialPreset::GIZMO_X_AXIS,
};

template<typename ObjectT>
ObjectT& spawn_transient(GameEngine& engine, std::shared_ptr<ObjectT>&& object)
{
	object->set_transient(true);
	auto* const result = object.get();
	engine.spawn_object(std::shared_ptr<Object>(std::move(object)));
	return *result;
}

TransformationComponent& transformation(GameEngine& engine, const Object& object)
{
	return engine.get_ecs().get_transformation(object.get_id());
}
}


//
// GizmoBase
//

GizmoBase::GizmoBase(GameEngine& engine, Gizmo& gizmo_) :
	engine(engine),
	gizmo(gizmo_)
{
	engine.get_ecs().add_transformation(get_id(), TransformationPersistence::Transient);
}

GizmoBase::~GizmoBase()
{
	engine.get_ecs().remove_transformation(get_id());
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

Object* GizmoBase::get_closest_clicked_axis(const Maths::Ray& ray) const
{
	const auto hit = engine.get_ecs().raycast(ray, axis_entities);
	return hit.bCollided ? &engine.get_ecs().get_object(hit.id) : nullptr;
}

void GizmoBase::register_colliders()
{
	for (std::size_t i = 0; i < axes.size(); ++i)
	{
		axis_entities[i] = axes[i]->get_id();
		engine.get_ecs().add_mesh_collider(axis_entities[i], ColliderPersistence::Transient);
	}
}

//
// TranslationGizmo
//

void TranslationGizmo::init()
{
	const std::array directions = { Maths::right_vec, Maths::up_vec, Maths::forward_vec };
	const std::array destinations = { &xAxis, &yAxis, &zAxis };
	for (size_t i = 0; i < destinations.size(); ++i)
	{
		auto renderable = Arrow::make_renderable(engine.get_ecs());
		renderable.material_owners[0] =
			engine.get_ecs().get_material_system().add(MaterialFactory::fetch_preset(axis_materials[i]));
		renderable.casts_shadow = false;
		renderable.render_on_top = true;
		auto axis = std::make_shared<Arrow>();
		auto& spawned = spawn_transient(engine, std::move(axis));
		engine.attach_renderable(spawned.get_id(), std::move(renderable));
		spawned.point(engine.get_ecs(), Maths::zero_vec, directions[i]);
		*destinations[i] = &spawned;
	}

	axes = {xAxis, yAxis, zAxis};
	for (auto* axis : axes)
	{
		transformation(engine, *axis).attach_to(get_id());
	}
	set_visibility(false);
}

bool TranslationGizmo::check_collision(const Maths::Ray& ray)
{
	if (!get_visibility())
	{
		return false;
	}

	// assume active_axis has been cleared already

	Object* closest_axis = get_closest_clicked_axis(ray);
	if (closest_axis)
	{
		active_axis = closest_axis;
		const auto& gizmo_transform = transformation(engine, *this);
		reference_transform.set_pos(gizmo_transform.get_position());
		reference_transform.set_orient(gizmo_transform.get_rotation());

		const glm::vec3 curr_axis =
			transformation(engine, *active_axis).get_rotation() * Maths::forward_vec;
		plane.normal = glm::normalize(glm::cross(curr_axis, glm::cross(curr_axis, ray.direction)));;
		plane.offset = gizmo_transform.get_position();
		p1 = Maths::ray_plane_intersection(ray, plane);
	}

	return active_axis;
}

void TranslationGizmo::process(const Maths::Ray& r1, const Maths::Ray& r2)
{
	if (!active_axis)
		return;

	const glm::vec3 curr_axis =
		transformation(engine, *active_axis).get_rotation() * Maths::forward_vec;

	const auto p2 = Maths::ray_plane_intersection(r2, plane);
	const auto Vp1_p2 = glm::dot(p2 - p1, curr_axis) * curr_axis;

	transformation(engine, gizmo).set_position(reference_transform.get_pos() + Vp1_p2);
}

//
// RotationGizmo
//

void RotationGizmo::init()
{
	const std::array destinations = { &xAxisNorm, &yAxisNorm, &zAxisNorm };
	for (size_t i = 0; i < destinations.size(); ++i)
	{
		auto renderable = ArcObject::make_renderable(engine.get_ecs());
		renderable.material_owners[0] = engine.get_ecs().get_material_system().add(
			MaterialFactory::fetch_preset(rotation_axis_materials[i]));
		renderable.casts_shadow = false;
		renderable.render_on_top = true;
		auto axis = std::make_shared<ArcObject>();
		auto& spawned = spawn_transient(engine, std::move(axis));
		engine.attach_renderable(spawned.get_id(), std::move(renderable));
		*destinations[i] = &spawned;
	}
	transformation(engine, *xAxisNorm).set_rotation(
		glm::angleAxis(-Maths::PI/2.0f, Maths::up_vec));
	transformation(engine, *yAxisNorm).set_rotation(
		glm::angleAxis(Maths::PI/2.0f, Maths::right_vec));

	axes = {xAxisNorm, yAxisNorm, zAxisNorm};
	for (auto* axis : axes)
	{
		transformation(engine, *axis).attach_to(get_id());
	}
	set_visibility(false);
}

bool RotationGizmo::check_collision(const Maths::Ray& ray)
{
	if (!get_visibility())
	{
		return false;
	}

	// assume active_axis has been cleared already

	Object* closest_axis = get_closest_clicked_axis(ray);
	if (closest_axis)
	{
		active_axis = closest_axis;
		const auto& gizmo_transform = transformation(engine, *this);
		reference_transform.set_pos(gizmo_transform.get_position());
		reference_transform.set_orient(gizmo_transform.get_rotation());

		plane.normal = glm::normalize(
			transformation(engine, *active_axis).get_rotation() * Maths::forward_vec);
		plane.offset = gizmo_transform.get_position();
		p1 = Maths::ray_plane_intersection(ray, plane);
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
	transformation(engine, gizmo).set_rotation(
		glm::normalize(quat * reference_transform.get_orient()));
}

//
// ScaleGizmo
//

ScaleGizmo::ScaleGizmo(GameEngine& engine, Gizmo& gizmo) :
	GizmoBase(engine, gizmo)
{
}

void ScaleGizmo::init()
{
	const std::array directions = { Maths::right_vec, Maths::up_vec, Maths::forward_vec };
	const std::array destinations = { &xAxis, &yAxis, &zAxis };
	for (size_t i = 0; i < destinations.size(); ++i)
	{
		auto renderable = ScaleGizmoObj::make_renderable(engine.get_ecs());
		renderable.material_owners[0] =
			engine.get_ecs().get_material_system().add(MaterialFactory::fetch_preset(axis_materials[i]));
		renderable.casts_shadow = false;
		renderable.render_on_top = true;
		auto axis = std::make_shared<ScaleGizmoObj>(directions[i]);
		auto& spawned = spawn_transient(engine, std::move(axis));
		engine.attach_renderable(spawned.get_id(), std::move(renderable));
		spawned.point(engine.get_ecs(), Maths::zero_vec, directions[i]);
		*destinations[i] = &spawned;
	}

	axes = {xAxis, yAxis, zAxis};
	for (auto* axis : axes)
	{
		transformation(engine, *axis).attach_to(get_id());
	}

	{
		MeshPtr cube_ptr = MeshFactory::cube();
		auto& cube = static_cast<ColorMesh&>(*cube_ptr);
		auto vertices = cube.get_vertices();
		auto indices = cube.get_indices();
		constexpr float CUBE_SIZE = 0.3f;
		transform_vertices(vertices, glm::scale(glm::mat4(1.0f), glm::vec3(CUBE_SIZE)));
		auto mesh_owner = engine.get_ecs().get_mesh_system().add(std::make_unique<ColorMesh>(std::move(vertices), std::move(indices)));
		auto renderable = Renderable::make_default(engine.get_ecs(), std::move(mesh_owner));
		auto material_owner = engine.get_ecs().get_material_system().add(
			MaterialFactory::fetch_preset(EMaterialPreset::GIZMO_UNIFORM_SCALE));
		renderable.material_owners[0] = std::move(material_owner);
		renderable.shading_mode = EShadingMode::UNLIT;
		renderable.casts_shadow = false;
		renderable.render_on_top = true;
		auto object = std::make_shared<Object>();
		uniformCube = &spawn_transient(engine, std::move(object));
		engine.attach_renderable(uniformCube->get_id(), std::move(renderable));
	}
	transformation(engine, *uniformCube).attach_to(get_id());

	set_visibility(false);
}

void ScaleGizmo::set_visibility(bool visibility)
{
	uniformCube->set_visibility(visibility);
	GizmoBase::set_visibility(visibility);
}

bool ScaleGizmo::check_collision(const Maths::Ray& ray)
{
	if (!get_visibility())
	{
		return false;
	}

	// assume active_axis has been cleared already
	uniform_scaling = false;

	const std::array uniform_candidate = { uniform_cube_entity };
	if (engine.get_ecs().raycast(ray, uniform_candidate).bCollided)
	{
		active_axis = uniformCube;
		uniform_scaling = true;
		reference_transform.set_scale(
			transformation(engine, *gizmo.selected_object).get_scale());

		plane.normal = glm::normalize(-ray.direction);
		plane.offset = transformation(engine, *this).get_position();
		p1 = Maths::ray_plane_intersection(ray, plane);
		return true;
	}

	Object* closest_axis = get_closest_clicked_axis(ray);
	if (closest_axis)
	{
		active_axis = closest_axis;
		reference_transform.set_scale(
			transformation(engine, *gizmo.selected_object).get_scale());

		const glm::vec3 curr_axis =
			transformation(engine, *active_axis).get_rotation() * Maths::forward_vec;
		plane.normal = glm::normalize(glm::cross(curr_axis, glm::cross(curr_axis, ray.direction)));
		plane.offset = transformation(engine, *this).get_position();
		p1 = Maths::ray_plane_intersection(ray, plane);
	}

	return active_axis;
}

void ScaleGizmo::register_colliders()
{
	GizmoBase::register_colliders();
	uniform_cube_entity = uniformCube->get_id();
	engine.get_ecs().add_mesh_collider(uniform_cube_entity, ColliderPersistence::Transient);
}

void ScaleGizmo::process(const Maths::Ray& r1, const Maths::Ray& r2)
{
	if (!active_axis)
		return;

	const auto p2 = Maths::ray_plane_intersection(r2, plane);

	if (uniform_scaling)
	{
		// use camera right vector so dragging left scales up, right scales down
		const glm::vec3 cam_right = glm::normalize(glm::cross(-plane.normal, Maths::up_vec));
		const float magnitude = -glm::dot(p2 - p1, cam_right);
		const float scale_factor = 1.0f + magnitude;
		const glm::vec3 new_scale = reference_transform.get_scale() * scale_factor;

		if (scale_factor <= 0.0f || glm::compMin(new_scale) < minimum_scale)
			return;

		transformation(engine, *gizmo.selected_object).set_scale(new_scale);
		return;
	}

	const glm::vec3 curr_axis =
		transformation(engine, *active_axis).get_rotation() * Maths::forward_vec;
	const float magnitude = glm::dot(p2 - p1, curr_axis);

	const auto& original_axis = static_cast<ScaleGizmoObj*>(active_axis)->original_axis;
	const glm::vec3 new_scale = reference_transform.get_scale() + original_axis * magnitude;

	if (glm::dot(new_scale, original_axis) < minimum_scale)
		return;

	transformation(engine, *gizmo.selected_object).set_scale(new_scale);
}

//
// Gizmo
//

Gizmo::Gizmo(GameEngine& engine) :
	engine(engine),
	translation(engine, *this),
	rotation(engine, *this),
	scale(engine, *this)
{
	engine.get_ecs().add_transformation(get_id(), TransformationPersistence::Transient);
	transformation(engine, translation).attach_to(get_id());
	transformation(engine, rotation).attach_to(get_id());
	transformation(engine, scale).attach_to(get_id());
}

Gizmo::~Gizmo()
{
	engine.get_ecs().remove_transformation(get_id());
}

void Gizmo::init()
{
	if (initialized)
		return;
	translation.init();
	rotation.init();
	scale.init();
	initialized = true;
	register_colliders();
}

void Gizmo::select_object(Object* obj)
{
	if (!obj || engine.get_game_mode() != EGameMode::EDITOR)
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
	auto& gizmo_transform = transformation(engine, *this);
	auto& selected_transform = transformation(engine, *selected_object);
	gizmo_transform.set_position(selected_transform.get_position());
	gizmo_transform.set_rotation(selected_transform.get_rotation());
	selected_transform.attach_to(gizmo_transform);

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

	engine.unhighlight_object(*selected_object);
	transformation(engine, *selected_object).detach_from();
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

void Gizmo::register_colliders()
{
	if (!initialized)
		return;
	translation.register_colliders();
	rotation.register_colliders();
	scale.register_colliders();
}

void Gizmo::delete_object()
{
	if (!selected_object)
		return;

	auto* obj = selected_object;
	deselect();
	engine.delete_object(obj->get_id());
}

void Gizmo::set_scale(const glm::vec3& new_scale)
{
	transformation(engine, translation).set_scale(new_scale);
	transformation(engine, rotation).set_scale(new_scale);
	transformation(engine, scale).set_scale(new_scale);
}
