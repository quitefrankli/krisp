#include "gui/gui_manager.hpp"
#include "game_engine.hpp"
#include "objects/objects.hpp"
#include "graphics_engine/graphics_engine.hpp"

#include <imgui.h>
#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <algorithm>


GuiGraphicsSettings::GuiGraphicsSettings() :
	light_ray(0.0f, 3.0f, 0.0f, 0.0f, -1.0f, 0.0f)
{
}

void GuiGraphicsSettings::draw()
{
	ImGui::Begin("Graphics Settings");

	ImGui::SliderFloat("lighting", &light_strength, 0.0f, 1.0f);
	ImGui::SliderFloat3("light position", glm::value_ptr(light_ray.origin), -50.0f, 50.0f);
	ImGui::SliderFloat3("light direction", glm::value_ptr(light_ray.direction), -1.0f, 1.0f);
	
	ImGui::End();
}

GuiObjectSpawner::GuiObjectSpawner()
{
	mapping = {
		{"cube", spawning_function_type([this](GameEngine& engine, bool textured){ 
			textured ? engine.spawn_object<Cube>(texture_path) : engine.spawn_object<Cube>(); })},
		{"sphere", spawning_function_type([this](GameEngine& engine, bool textured){ 
			textured ? engine.spawn_object<Sphere>() : engine.spawn_object<Sphere>(); })}
	};
}

void GuiObjectSpawner::draw()
{
	ImGui::Begin("Object Spawner");
	
	ImGui::Checkbox("textured", &use_texture);

	ImVec2 button_dim(button_width, button_height);
	if (spawning_function)
	{
		std::for_each(mapping.begin(), mapping.end(), [&](auto& pair){ ImGui::Button(pair.first.c_str(), button_dim); });
	} else {
		for (auto& [key, value] : mapping)
		{
			if (ImGui::Button(key.c_str(), button_dim))
			{
				spawning_function = &value;
			}
		}
	}
	
	ImGui::End();
}

void GuiObjectSpawner::process(GameEngine& engine)
{
	if (spawning_function)
	{
		(*spawning_function)(engine, use_texture);
		spawning_function = nullptr;
	}	
}