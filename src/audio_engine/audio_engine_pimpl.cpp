#include "audio_engine_pimpl.hpp"
#include "audio_engine.hpp"


AudioEnginePimpl::AudioEnginePimpl() :
	audio_engine(std::make_unique<AudioEngine>())
{
}

AudioEnginePimpl::~AudioEnginePimpl() = default;

void AudioEnginePimpl::play(const std::string_view audiofile)
{
}
