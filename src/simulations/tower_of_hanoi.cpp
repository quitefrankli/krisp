#include "tower_of_hanoi.hpp"

#include "graphics_engine/graphics_engine.hpp"
#include "graphics_engine/graphics_engine_commands.hpp"
#include "game_engine.hpp"

#include <glm/glm.hpp>
#include <glm/ext.hpp>

#include <iostream>


TowerOfHanoi::TowerOfHanoi(GameEngine& engine_) : Simulation(engine_)
{
	HollowCylinder cyl1, cyl2, cyl3;
	cyl1.set_scale(glm::vec3(0.6f));
	cyl2.set_scale(glm::vec3(0.8f));
	cyl1.set_position(glm::vec3(-1.0f, 0.0f, 0.0f));
	cyl3.set_position(glm::vec3(1.0f, 0.0f, 0.0f));

	donuts.push_back(std::make_shared<Object>(std::move(cyl1)));
	donuts.push_back(std::make_shared<Object>(std::move(cyl2)));
	donuts.push_back(std::make_shared<Object>(std::move(cyl3)));

	for (auto& donut : donuts)
	{
		SpawnObjectCmd cmd;
		cmd.object = donut;
		cmd.object_id = donut->get_id();
		engine.get_graphics_engine().enqueue_cmd(std::make_unique<SpawnObjectCmd>(std::move(cmd)));
	}

	for (int i = 0; i < 3; i++)
	{
		Cylinder pillar;
		pillar.set_scale(glm::vec3(0.3f, 2.0f, 0.3f));
		pillar.set_position(glm::vec3(i-1, 0.0f, 0.0f));
		pillars.push_back(std::make_shared<Object>(std::move(pillar)));
		SpawnObjectCmd cmd;
		cmd.object = pillars.back();
		cmd.object_id = pillars.back()->get_id();
		engine.get_graphics_engine().enqueue_cmd(std::make_unique<SpawnObjectCmd>(std::move(cmd)));
	}
}

void TowerOfHanoi::start()
{
	SequentialAnimation animation;
	std::vector<glm::vec3> translations = {
		glm::vec3(0.0f, 1.0f, 0.0f),
		glm::vec3(1.0f, 0.0f, 0.0f),
		glm::vec3(0.0f, -1.0f, 0.0f)
	};
	animation.animations.push(Animation());
	animation.animations.back().final_transform = donuts[0]->get_transform();
	animation.animations.back().duration = -1.0f;

	for (auto& translation : translations)
	{
		Animation keyframe;
		keyframe.object = donuts[0];
		keyframe.initial_transform = animation.animations.back().final_transform;
		keyframe.final_transform = glm::translate(glm::mat4(1.0f), translation) * animation.animations.back().final_transform;
		keyframe.duration = 2.0f;
		animation.animations.push(std::move(keyframe));
	}
	engine.animator.add_animation(std::move(animation));
}