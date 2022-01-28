#include "gizmo.hpp"

#include "objects/objects.hpp"
#include "graphics_engine/graphics_engine_commands.hpp"
#include "graphics_engine/graphics_engine.hpp"
#include "game_engine.hpp"
#include "shapes/shapes.hpp"


Gizmo::Gizmo(GameEngine& engine_) :
	engine(engine_)
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

void TranslationGizmo::init()
{
	glm::vec3 O(0.0f);
	xAxis.point(O, glm::vec3(1.0f, 0.0f, 0.0f));
	yAxis.point(O, glm::vec3(0.0f, 1.0f, 0.0f));
	zAxis.point(O, glm::vec3(0.0f, 0.0f, -1.0f));

	engine.get_graphics_engine().enqueue_cmd(std::make_unique<SpawnObjectCmd>(xAxis));
	engine.get_graphics_engine().enqueue_cmd(std::make_unique<SpawnObjectCmd>(yAxis));
	engine.get_graphics_engine().enqueue_cmd(std::make_unique<SpawnObjectCmd>(zAxis));

	shapes.emplace_back<Shapes::Cylinder>(10);
	engine.get_graphics_engine().enqueue_cmd(std::make_unique<SpawnObjectCmd>(*this));

	xAxis.attach_to(this);
	yAxis.attach_to(this);
	zAxis.attach_to(this);
}