#pragma once

#include "objects/object.hpp"


class Gizmo : public Object
{
public:
	Gizmo();

	void attach(Object* object);
	void attach(Object& object);

private:
	Object* object = nullptr;
};

class TranslationGizmo : public Gizmo
{
private:
	// these could be maybe useful in the future
	Object xAxis;
	Object yAxis;
	Object zAxis;
};