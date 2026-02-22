#include "tile_system.hpp"

#include <game_engine.hpp>
#include <graphics_engine/graphics_engine.hpp>
#include <iapplication.hpp>
#include <resource_loader.hpp>
#include <utility.hpp>
#include <camera.hpp>
#include <config.hpp>
#include <renderable/mesh_factory.hpp>

#include <fmt/core.h>
#include <fmt/color.h>
#include <glm/gtx/string_cast.hpp>
#include <imgui.h>

#include <GLFW/glfw3.h> // for key macros

#include <iapplication.hpp>
#include <game_engine.hpp>

#include <iostream>


class Application : public IApplication
{
public:
	virtual void on_tick(GameEngine& engine, float delta) override
	{
		const auto hovered = engine.get_ecs().check_any_entity_hovered(engine.get_mouse_ray());
		Object* object_to_unhighlight = prev_hovered.has_value() ?
			&engine.get_ecs().get_object(prev_hovered.value()) : nullptr;

		if (hovered.bCollided)
		{
			const auto& hovered_obj = engine.get_ecs().get_object(hovered.id);
			if (&hovered_obj == object_to_unhighlight)
			{
				object_to_unhighlight = nullptr;
			} else
			{
				engine.highlight_object(hovered_obj);
			}
			prev_hovered = hovered.id;
		} else
		{
			prev_hovered.reset();
		}

		if (object_to_unhighlight)
		{
			engine.unhighlight_object(*object_to_unhighlight);
		}
	}
	virtual void on_click(GameEngine&, Object&) override {}
	virtual void on_begin(GameEngine&) override {}
	virtual void on_key_press(GameEngine&, const KeyInput&) override {}

private:
	std::optional<ObjectID> prev_hovered;
};

int main(int argc, char* argv[])
{
	Config::init(PROJECT_NAME);
	auto engine = GameEngine::create<Application>();

	Tile::tile_object_spawner = [&engine](const TileCoord& coord)
	{
		Object& obj = engine.spawn_object<Object>(Renderable::make_default(MeshFactory::cube_id()));
		obj.set_position(glm::vec3(coord.x, -0.5f, coord.y));
		obj.set_scale(glm::vec3(0.95f, 0.95f, 0.95f));
		
		// engine.get_ecs().add_collider(obj.get_id(), std::make_unique<AABB>(obj.get_position(), obj.get_scale());
		engine.get_ecs().add_collider(obj.get_id(), std::make_unique<SphereCollider>());
		engine.get_ecs().add_hoverable_entity(obj.get_id());
	};
	TileSystem tile_system;

	engine.run();
	fmt::print(fg(fmt::color::green), "Clean shutdown success!\n");
}