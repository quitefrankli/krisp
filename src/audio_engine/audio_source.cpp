#include "audio_source.hpp"
#include "audio_engine.hpp"
#include "utility.hpp"

#include <OpenAL/al.h>

#include <iostream>


AudioSource::AudioSource(AudioEngine& audio_engine) :
	audio_engine(audio_engine)
{
	alGenSources(1, &p_Source);
	alSourcef(p_Source, AL_PITCH, p_Pitch);
	alSourcef(p_Source, AL_GAIN, p_Gain);
	alSource3f(p_Source, AL_POSITION, position[0], position[1], position[2]);
	alSource3f(p_Source, AL_VELOCITY, velocity[0], velocity[1], velocity[2]);
	alSourcei(p_Source, AL_LOOPING, p_LoopSound);
	alSourcei(p_Source, AL_BUFFER, p_Buffer);
	// alSourcei(p_Source, AL_SOURCE_RELATIVE, 0); // allows to work with listener
	alSourcef(p_Source, AL_ROLLOFF_FACTOR, 0.2f); // default = 1
}

AudioSource::AudioSource(AudioSource&& other) noexcept : audio_engine(other.audio_engine)
{
	p_Source = other.p_Source;
	p_Pitch = other.p_Pitch;
	p_Gain = other.p_Gain;
	position = other.position;
	velocity = other.velocity;
	p_LoopSound = other.p_LoopSound;
	p_Buffer = other.p_Buffer;

	other.should_destroy = false;
}

AudioSource::~AudioSource()
{
	if (!should_destroy)
	{
		return;
	}
	alDeleteSources(1, &p_Source);
}

void AudioSource::set_audio(const std::string_view filename)
{
	set_audio_buffer(audio_engine.get_buffer(filename.data()));
}

void AudioSource::set_audio_buffer(const uint32_t buffer)
{
	ALint state = AL_PLAYING;
	alGetSourcei(p_Source, AL_SOURCE_STATE, &state);
	if (state == AL_PLAYING || state == AL_PAUSED)
	{
		alSourceStop(p_Source);
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
		std::cout << "AudioSource::play: warning attempted to play while something was already playing\n";
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

void AudioSource::set_gain(float gain)
{
	if (gain == p_Gain)
		return;
	p_Gain = gain;
	alSourcef(p_Source, AL_GAIN, gain);
}

void AudioSource::set_pitch(float pitch)
{
	if (pitch == p_Pitch)
		return;
	p_Pitch = pitch;
	// pitch seems quite sensitive
	const float true_pitch = 1.0f + (pitch - 1.0f) * 0.2f;
	alSourcef(p_Source, AL_PITCH, true_pitch);
}

void AudioSource::set_position(const glm::vec3& position)
{
	if (position == this->position)
		return;
	this->position = position;
	alSource3f(p_Source, AL_POSITION, position.x, position.y, position.z);
}
