#pragma once

#include "audio_source.hpp"

#include <memory>


class AudioEngine;

class AudioEnginePimpl
{
public:
	AudioEnginePimpl();
	~AudioEnginePimpl();

	AudioSource create_source();

private:
	std::unique_ptr<AudioEngine> audio_engine;
};