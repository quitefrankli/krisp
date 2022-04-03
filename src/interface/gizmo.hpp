#pragma once

#include "objects/objects.hpp"
#include "maths.hpp"
#include "objects.hpp"

#include <array>


class GameEngine;
class Gizmo;

class GizmoBase : public Object
{
public:
	GizmoBase(GameEngine& engine, Gizmo& gizmo);

	virtual void init() = 0;
	virtual void set_visibility(bool) override;
	virtual bool check_collision(const Maths::Ray& ray) = 0;
	void clear_active_axis() { active_axis = nullptr; }
	
protected:
	GameEngine& engine;
	Gizmo& gizmo;
	virtual bool is_essential_child(Object* child);
	Maths::TransformationComponents reference_transform;
	std::array<Object*, 3> axes;
	Object* active_axis = nullptr; // when axis is clicked on
	Maths::Plane plane; // plane of interaction
	glm::vec3 p1; // point on plane for first intersection
};

class TranslationGizmo : public GizmoBase
{
public:
	using GizmoBase::GizmoBase;
	virtual void init() override;
	virtual bool check_collision(const Maths::Ray& ray) override;
	void process(const Maths::Ray& r1, const Maths::Ray& r2);
	
private:
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
	virtual bool check_collision(const Maths::Ray& ray) override;

private:
	// represents the normal of the arc
	Arc xAxisNorm;
	Arc yAxisNorm;
	Arc zAxisNorm;
};

class ScaleGizmo : public GizmoBase
{
public:
	using GizmoBase::GizmoBase;
	virtual void init() override;
	virtual bool check_collision(const Maths::Ray& ray) override;
	void process(const Maths::Ray& r1, const Maths::Ray& r2);
	
private:
	ScaleGizmoObj xAxis;
	ScaleGizmoObj yAxis;
	ScaleGizmoObj zAxis;
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
	virtual bool check_collision(const Maths::Ray& ray) override;
	void delete_object();

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
	friend ScaleGizmo;
};