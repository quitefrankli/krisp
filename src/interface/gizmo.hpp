#pragma once

#include "objects/objects.hpp"
#include "maths.hpp"
#include "objects.hpp"

#include <array>


class Gizmo;
class GameEngine;

class GizmoBase : public Object
{
public:
	GizmoBase(GameEngine& engine, Gizmo& gizmo);

	virtual void init() = 0;
	virtual void set_visibility(bool) override;
	void clear_active_axis() { active_axis = nullptr; }
	virtual void register_colliders();
	
protected:
	Object* get_closest_clicked_axis(const Maths::Ray& ray) const;

protected:
	GameEngine& engine;
	Gizmo& gizmo;
	virtual bool is_essential_child(Object* child);
	Maths::Transform reference_transform;
	std::array<Object*, 3> axes;
	std::array<EntityID, 3> axis_entities;
	Object* active_axis = nullptr; // when axis is clicked on
	Maths::Plane plane; // plane of interaction
	glm::vec3 p1; // point on plane for first intersection
};

class TranslationGizmo : public GizmoBase
{
public:
	using GizmoBase::GizmoBase;
	virtual void init() override;
	bool check_collision(const Maths::Ray& ray);
	void process(const Maths::Ray& r1, const Maths::Ray& r2);
	
private:
	friend Gizmo;

	Arrow xAxis;
	Arrow yAxis;
	Arrow zAxis;
};

class RotationGizmo : public GizmoBase
{
public:
	using GizmoBase::GizmoBase;
	virtual void init() override;
	void process(const Maths::Ray& r1, const Maths::Ray& r2);
	bool check_collision(const Maths::Ray& ray);

private:
	friend Gizmo;

	// represents the normal of the arc
	ArcObject xAxisNorm;
	ArcObject yAxisNorm;
	ArcObject zAxisNorm;
};

class ScaleGizmo : public GizmoBase
{
public:
	ScaleGizmo(GameEngine& engine, Gizmo& gizmo);

	virtual void init() override;
	virtual void set_visibility(bool) override;
	bool check_collision(const Maths::Ray& ray);
	void register_colliders() override;
	void process(const Maths::Ray& r1, const Maths::Ray& r2);
	
private:
	friend Gizmo;

	ScaleGizmoObj xAxis;
	ScaleGizmoObj yAxis;
	ScaleGizmoObj zAxis;
	Object uniformCube;
	EntityID uniform_cube_entity;

	const float minimum_scale = 0.1f;
	bool uniform_scaling = false;
};

class Gizmo : public Object
{
public:
	Gizmo(GameEngine& engine);
	void init();
	void select_object(Object* obj);
	void deselect();
	bool is_active() { return isActive; }
	// r1 is first mouse pos, and r2 is second mouse pos
	void process(const Maths::Ray& r1, const Maths::Ray& r2);
	bool check_collision(const Maths::Ray& ray);
	void register_colliders();
	void delete_object();
	Object* get_selected_object() { return selected_object; }
	virtual void set_scale(const glm::vec3& new_scale) override;

private:
	bool scale_mode = false;
	void toggle_mode();

	TranslationGizmo translation;
	RotationGizmo rotation;
	ScaleGizmo scale;
	Object* selected_object = nullptr;
	// GizmoBase* active_gizmo = nullptr;
	bool isActive = false; // when gizmo is selected
	GameEngine& engine;
	bool initialized = false;
	friend ScaleGizmo;
};
