#pragma once

#include "game_engine.hpp"
#include "hot_reload.hpp"
#include "objects/cubemap.hpp"
#include "objects/light_source.hpp"
#include "objects/objects.hpp"
#include "resource_loader.hpp"
#include "audio_engine/audio_source.hpp"
#include "audio_engine/audio_engine_pimpl.hpp"
#include "utility.hpp"

#include <iostream>


static int tmp = 0;
static uint64_t tmp2;
class Experimental
{
public:
	Experimental(GameEngine& engine_) : 
		engine(engine_),
		audio_source(engine_.get_audio_engine().create_source())
	{
	}

	// manually triggered
	void process()
	{
		audio_source.set_audio(Utility::get().get_audio_path().string() + "/music1.wav");
		audio_source.play();
	}

	// game engine triggers this periodically
	// time_delta is in seconds
	void process(float time_delta)
	{
		return; // uncomment to enable

		time_elapsed += time_delta;
		if (time_elapsed > 1.0f)
		{
			time_elapsed = 0;
		}
		
		// glm::vec3 axis;
		// HotReload::get().slfp->gen_vec(axis);
		// glm::quat rotator = glm::angleAxis(0.02f, glm::normalize(axis));
		// cube1.set_rotation(rotator * cube1.get_rotation());
	}

private:
	GameEngine& engine;
	float time_elapsed = 0;
	Cube cube;
	AudioSource audio_source;
};