#pragma once

#include "objects/objects.hpp"


class GameEngine;

class Gizmo : public Object
{
public:
	Gizmo(GameEngine& engine);

	virtual void init() = 0;
	virtual void detach_all_children() override;

protected:
	GameEngine& engine;
	virtual void on_child_attached(Object* child) override;
	virtual void on_child_detached(Object* child) override;
	virtual bool is_essential_child(Object* child) = 0;

private:
	Object* object = nullptr;
};

class TranslationGizmo : public Gizmo
{
public:
	using Gizmo::Gizmo;
	
	void init() override;

private:
	virtual bool is_essential_child(Object* child) override;

	Arrow xAxis;
	Arrow yAxis;
	Arrow zAxis;
};