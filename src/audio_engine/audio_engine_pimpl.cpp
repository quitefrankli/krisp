#include "audio_engine_pimpl.hpp"
#include "audio_engine.hpp"


AudioEnginePimpl::AudioEnginePimpl() :
	audio_engine(std::make_unique<AudioEngine>())
{
}

AudioEnginePimpl::~AudioEnginePimpl() = default;

AudioSource AudioEnginePimpl::create_source()
{
	return AudioSource(*audio_engine);
}