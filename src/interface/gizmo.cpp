#include "gizmo.hpp"

#include "objects/objects.hpp"


Gizmo::Gizmo()
{

}

void Gizmo::attach(Object* object)
{
	this->object = object;
}

void Gizmo::attach(Object& object)
{
	attach(&object);
}