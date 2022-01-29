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

void Gizmo::detach_all_children()
{
	// can't do simple for loop since we may be removing elements while iterating
	for (auto child = children.begin(), next_child = child; child != children.end(); child = next_child)
	{
		next_child++;
		if (is_essential_child(child->second))
		{
			continue;
		}
		child->second->detach_from();
	}
}

void Gizmo::on_child_attached(Object* child)
{
	if (is_essential_child(child))
	{
		return;
	}

	// gizmo can only have 1 non-essential child, so first detach everything not essential
	detach_all_children();

	set_visibility(true);

	set_position(child->get_position());
	set_rotation(child->get_rotation());
}

void Gizmo::on_child_detached(Object* child)
{
	set_visibility(false);
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

	set_visibility(false);
}

bool TranslationGizmo::is_essential_child(Object* child)
{
	return child == &xAxis || child == &yAxis || child == &zAxis;
}