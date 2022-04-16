#include "audio_source.hpp"

#include <AL\al.h>

#include <iostream>


AudioSource::AudioSource(AudioEngine& audio_engine) :
	audio_engine(audio_engine)
{
	alGenSources(1, &p_Source);
	alSourcef(p_Source, AL_PITCH, p_Pitch);
	alSourcef(p_Source, AL_GAIN, p_Gain);
	alSource3f(p_Source, AL_POSITION, p_Position[0], p_Position[1], p_Position[2]);
	alSource3f(p_Source, AL_VELOCITY, p_Velocity[0], p_Velocity[1], p_Velocity[2]);
	alSourcei(p_Source, AL_LOOPING, p_LoopSound);
	alSourcei(p_Source, AL_BUFFER, p_Buffer);
}

AudioSource::~AudioSource()
{
	alDeleteSources(1, &p_Source);
}

void AudioSource::set_audio_buffer(const uint32_t buffer)
{
	ALint state = AL_PLAYING;
	alGetSourcei(p_Source, AL_SOURCE_STATE, &state);
	if (state == AL_PLAYING)
	{
		std::cout << "AudioSource::play: currently playing\n";
		return;
	}

	if (buffer != p_Buffer)
	{
		p_Buffer = buffer;
		alSourcei(p_Source, AL_BUFFER, (ALint)p_Buffer);
	}
}

void AudioSource::play()
{
	ALint state;
	alGetSourcei(p_Source, AL_SOURCE_STATE, &state);
	if (state == AL_PLAYING)
	{
		std::cout << "AudioSource::play: currently playing\n";
		return;
	}

	alSourcePlay(p_Source);
	// std::cout << "playing sound\n";
	// while (state == AL_PLAYING && alGetError() == AL_NO_ERROR)
	// {
	// 	std::cout << "currently playing sound\n";
	// 	alGetSourcei(p_Source, AL_SOURCE_STATE, &state);
	// }
	// std::cout << "done playing sound\n";
}