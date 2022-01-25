#include "tower_of_hanoi.hpp"

#include "graphics_engine/graphics_engine.hpp"
#include "graphics_engine/graphics_engine_commands.hpp"
#include "game_engine.hpp"

#include <glm/glm.hpp>
#include <glm/ext.hpp>

#include <iostream>


TowerOfHanoi::TowerOfHanoi(GameEngine& engine_) : Simulation(engine_)
{
	for (int i = 0; i < 3; i++)
	{
		pillar_t pillar;
		pillar.set_scale(glm::vec3(0.3f, 2.0f, 0.3f));
		pillar.set_position(glm::vec3(i-1, 0.0f, 0.0f));
		pillars.push_back(std::make_shared<pillar_t>(std::move(pillar)));
		auto cmd = std::make_unique<SpawnObjectCmd>(pillars.back(), pillars.back()->get_id());
		engine.get_graphics_engine().enqueue_cmd(std::move(cmd));
	}

	const int NUM_DONUTS = 3;
	for (int i = 0; i < NUM_DONUTS; i++)
	{
		donut_t donut;
		float scaler = 1.0f - (float)i / 10.0f;
		donut.set_scale(glm::vec3(scaler, 1.0f, scaler));
		donut.set_position(glm::vec3(-1.0f, pillars[0]->content_height, 0.0f));
		pillars[0]->content_height += donut.get_scale()[1] * 0.5f;
		donuts.push_back(std::make_shared<donut_t>(std::move(donut)));
		auto cmd = std::make_unique<SpawnObjectCmd>(donuts.back(), donuts.back()->get_id());
		engine.get_graphics_engine().enqueue_cmd(std::move(cmd));
	}
}

static int counter = 0;
void TowerOfHanoi::start()
{
	switch (counter)
	{
		case 0:
			move_donut(donuts[2], 2);
			break;
		case 1:
			move_donut(donuts[1], 1);
			break;
		case 2:
			move_donut(donuts[2], -1);
			break;
		case 3:
			move_donut(donuts[0], 2);
			break;
		case 4:
			move_donut(donuts[2], -1);
			break;
		case 5:
			move_donut(donuts[1], 1);
			break;
		case 6:
			move_donut(donuts[2], 2);
			break;
		default:
			break;
	}
	counter++;
}

void TowerOfHanoi::move_donut(std::shared_ptr<donut_t>& donut, int vector)
{
	if (donut->pillar_index + vector > 2 || donut->pillar_index + vector < 0)
	{
		std::cout << "TowerOfHanoi: cannot move donut past pillars!\n";
		return;
	}

	SequentialAnimation sequence;
	Animation anim1, anim2, anim3;

	// move donut up out of pillar
	anim1.object = donut;
	anim1.duration = 1.0f;
	anim1.initial_transform = donut->get_transform();
	anim1.final_transform = anim1.initial_transform;
	anim1.final_transform[3][1] = 2.5f;
	pillars[donut->pillar_index]->content_height -= donut->get_scale()[1] * 0.5f;

	// move donut left or right
	anim2.object = donut;
	anim2.duration = 0.7f;
	anim2.initial_transform = anim1.final_transform;
	anim2.final_transform = anim2.initial_transform;
	anim2.final_transform[3][0] += vector;

	// move donut down onto new pillar
	anim3.object = donut;
	anim3.duration = 1.0f;
	anim3.initial_transform = anim2.final_transform;
	anim3.final_transform = anim3.initial_transform;
	donut->pillar_index += vector;
	anim3.final_transform[3][1] = pillars[donut->pillar_index]->content_height;
	pillars[donut->pillar_index]->content_height += donut->get_scale()[1] * 0.5f;

	sequence.animations.push(std::move(anim1));
	sequence.animations.push(std::move(anim2));
	sequence.animations.push(std::move(anim3));
	engine.animator.add_animation(std::move(sequence));	
}