#pragma once

#include <cstdint>
#include <string_view>


class AudioEngine;

class AudioSource
{
public:
	AudioSource(AudioEngine& audio_engine);
	~AudioSource();

	void set_audio_buffer(const uint32_t buffer);
	void play();

private:

	uint32_t p_Source;
	float p_Pitch = 1.f;
	float p_Gain = 1.f;
	float p_Position[3] = { 0,0,0 };
	float p_Velocity[3] = { 0,0,0 };
	bool p_LoopSound = false;
	uint32_t p_Buffer = 0;
	AudioEngine& audio_engine;
};

