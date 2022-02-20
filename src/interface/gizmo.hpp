#pragma once

#include "objects/objects.hpp"
#include "maths.hpp"

#include <array>


class GameEngine;

class Gizmo : public Object
{
public:
	Gizmo(GameEngine& engine);

	virtual void init() = 0;
	virtual void detach_all_children() override;
	bool is_active() { return isActive; }
	
protected:
	GameEngine& engine;
	virtual void on_child_attached(Object* child) override;
	virtual void on_child_detached(Object* child) override;
	virtual bool is_essential_child(Object* child) = 0;
	bool isActive = false;
	Maths::TransformationComponents reference_transform;
	std::array<Object*, 3> axes;

private:
	Object* object = nullptr;
};

class TranslationGizmo : public Gizmo
{
public:
	using Gizmo::Gizmo;
	
	void init() override;

	virtual bool check_collision(Maths::Ray& ray) override;

	void process(const glm::vec3& dir, float magnitude);
private:
	virtual bool is_essential_child(Object* child) override;

	Arrow xAxis;
	Arrow yAxis;
	Arrow zAxis;

	glm::vec3 curr_axis;
};