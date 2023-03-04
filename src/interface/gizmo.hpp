#pragma once

#include "objects/objects.hpp"
#include "maths.hpp"
#include "objects.hpp"
#include "objects/object_interfaces/clickable.hpp"

#include <array>


template<typename GameEngineT>
class Gizmo;

template<typename GameEngineT>
class GizmoBase : public Object
{
public:
	GizmoBase(GameEngineT& engine, Gizmo<GameEngineT>& gizmo);

	virtual void init() = 0;
	virtual void set_visibility(bool) override;
	virtual bool check_collision(const Maths::Ray& ray) override { return false; };
	void clear_active_axis() { active_axis = nullptr; }
	
protected:
	GameEngineT& engine;
	Gizmo<GameEngineT>& gizmo;
	virtual bool is_essential_child(Object* child);
	Maths::Transform reference_transform;
	std::array<Object*, 3> axes;
	Object* active_axis = nullptr; // when axis is clicked on
	Maths::Plane plane; // plane of interaction
	glm::vec3 p1; // point on plane for first intersection
};

template<typename GameEngineT>
class TranslationGizmo : public GizmoBase<GameEngineT>
{
public:
	using GizmoBase<GameEngineT>::GizmoBase;
	virtual void init() override;
	virtual bool check_collision(const Maths::Ray& ray) override;
	void process(const Maths::Ray& r1, const Maths::Ray& r2);
	
private:
	using GizmoBase<GameEngineT>::engine;
	using GizmoBase<GameEngineT>::gizmo;
	using GizmoBase<GameEngineT>::is_essential_child;
	using GizmoBase<GameEngineT>::reference_transform;
	using GizmoBase<GameEngineT>::axes;
	using GizmoBase<GameEngineT>::active_axis;
	using GizmoBase<GameEngineT>::plane;
	using GizmoBase<GameEngineT>::p1;

	using Object::get_visibility;
	using Object::set_visibility;
	using Object::get_position;
	using Object::get_rotation;

	friend Gizmo<GameEngineT>;

	Arrow xAxis;
	Arrow yAxis;
	Arrow zAxis;
};

template<typename GameEngineT>
class RotationGizmo : public GizmoBase<GameEngineT>
{
public:
	using GizmoBase<GameEngineT>::GizmoBase;
	virtual void init() override;
	void process(const Maths::Ray& r1, const Maths::Ray& r2);
	virtual bool check_collision(const Maths::Ray& ray) override;

private:
	using GizmoBase<GameEngineT>::engine;
	using GizmoBase<GameEngineT>::gizmo;
	using GizmoBase<GameEngineT>::is_essential_child;
	using GizmoBase<GameEngineT>::reference_transform;
	using GizmoBase<GameEngineT>::axes;
	using GizmoBase<GameEngineT>::active_axis;
	using GizmoBase<GameEngineT>::plane;
	using GizmoBase<GameEngineT>::p1;

	using Object::get_visibility;
	using Object::set_visibility;
	using Object::get_position;
	using Object::get_rotation;

	friend Gizmo<GameEngineT>;

	// represents the normal of the arc
	Arc xAxisNorm;
	Arc yAxisNorm;
	Arc zAxisNorm;
};

template<typename GameEngineT>
class ScaleGizmo : public GizmoBase<GameEngineT>
{
public:
	ScaleGizmo(GameEngineT& engine, Gizmo<GameEngineT>& gizmo);

	virtual void init() override;
	virtual bool check_collision(const Maths::Ray& ray) override;
	void process(const Maths::Ray& r1, const Maths::Ray& r2);
	
private:
	using GizmoBase<GameEngineT>::engine;
	using GizmoBase<GameEngineT>::gizmo;
	using GizmoBase<GameEngineT>::is_essential_child;
	using GizmoBase<GameEngineT>::reference_transform;
	using GizmoBase<GameEngineT>::axes;
	using GizmoBase<GameEngineT>::active_axis;
	using GizmoBase<GameEngineT>::plane;
	using GizmoBase<GameEngineT>::p1;

	using Object::get_visibility;
	using Object::set_visibility;
	using Object::get_position;
	using Object::get_rotation;

	friend Gizmo<GameEngineT>;

	ScaleGizmoObj xAxis;
	ScaleGizmoObj yAxis;
	ScaleGizmoObj zAxis;

	const float minimum_scale = 0.1f;
};

template<typename GameEngineT>
class Gizmo : public Object, public OnClickDispatchers::IBaseDispatcher
{
public:
	Gizmo(GameEngineT& engine);
	void init();
	void select_object(Object* obj);
	void deselect();
	bool is_active() { return isActive; }
	// r1 is first mouse pos, and r2 is second mouse pos
	void process(const Maths::Ray& r1, const Maths::Ray& r2);
	virtual bool check_collision(const Maths::Ray& ray) override;
	virtual void dispatch_on_click(Object& object, const Maths::Ray& ray, const glm::vec3& intersection) override;
	void delete_object();

private:
	bool scale_mode = false;
	void toggle_mode();

	TranslationGizmo<GameEngineT> translation;
	RotationGizmo<GameEngineT> rotation;
	ScaleGizmo<GameEngineT> scale;
	Object* selected_object = nullptr;
	// GizmoBase* active_gizmo = nullptr;
	bool isActive = false; // when gizmo is selected
	GameEngineT& engine;
	friend ScaleGizmo<GameEngineT>;
};