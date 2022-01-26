#pragma once

#include "objects/objects.hpp"


class GameEngine;

class Gizmo : public Object
{
public:
	Gizmo(GameEngine& engine);

	virtual void init() = 0;

	void attach(Object* object);
	void attach(Object& object);

protected:
	GameEngine& engine;

private:
	Object* object = nullptr;
};

class TranslationGizmo : public Gizmo
{
public:
	using Gizmo::Gizmo;
	
	void init() override;

private:
	// these could be maybe useful in the future
	Arrow xAxis;
	Arrow yAxis;
	Arrow zAxis;
};