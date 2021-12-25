#include "gui.hpp"

#include <imgui.h>
#include <glm/gtc/type_ptr.hpp>

#include <iostream>


GuiGraphicsSettings::GuiGraphicsSettings() :
	light_ray(0.0f, 10.0f, 0.0f, 0.0f, -1.0f, 0.0f)
{
}

void GuiGraphicsSettings::draw()
{
	ImGui::Begin("Graphics Settings");

	ImGui::SliderFloat("lighting", &light_strength, 0.0f, 1.0f);
	ImGui::SliderFloat3("light position", glm::value_ptr(light_ray.origin), -100.0f, 100.0f);
	ImGui::SliderFloat3("light direction", glm::value_ptr(light_ray.direction), -1.0f, 1.0f);
	
	ImGui::End();
}

GuiObjectSpawner::GuiObjectSpawner()
{
	mapping = {
		{"cube", false},
		{"sphere", false}
	};
}

void GuiObjectSpawner::draw()
{
	ImGui::Begin("Object Spawner");
	
	ImVec2 button_dim(button_width, button_height);
	for (auto& [key, value] : mapping)
	{
		value = value | ImGui::Button(key.c_str(), button_dim);
	}
	
	ImGui::End();
}