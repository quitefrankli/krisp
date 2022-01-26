#include "gizmo.hpp"

#include "objects/objects.hpp"
#include "graphics_engine/graphics_engine_commands.hpp"
#include "graphics_engine/graphics_engine.hpp"
#include "game_engine.hpp"


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

	auto cmd = std::make_unique<SpawnObjectCmd>(xAxis, get_id());
	engine.get_graphics_engine().enqueue_cmd(std::move(cmd));

	cmd = std::make_unique<SpawnObjectCmd>(yAxis, get_id());
	engine.get_graphics_engine().enqueue_cmd(std::move(cmd));

	cmd = std::make_unique<SpawnObjectCmd>(zAxis, get_id());
	engine.get_graphics_engine().enqueue_cmd(std::move(cmd));
}