#pragma once

#include <memory>
#include <string_view>


class AudioEngine;

class AudioEnginePimpl
{
public:
	AudioEnginePimpl();
	~AudioEnginePimpl();
	void play(const std::string_view audiofile);

private:
	std::unique_ptr<AudioEngine> audio_engine;
};