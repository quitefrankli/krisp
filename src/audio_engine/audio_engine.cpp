#include "audio_engine.hpp"

// #include <AL/al.h>
// #include <AL/alc.h>
#include <sndfile.h>

#include <stdexcept>
#include <iostream>


AudioEngine::AudioEngine()
{
	// uint32_t /*ALuint*/ sound1 = SoundBuffer::get()->addSoundEffect("../resources/sound/spell.ogg");
	// uint32_t /*ALuint*/ sound2 = SoundBuffer::get()->addSoundEffect("../resources/sound/magicfail.ogg");

	// SoundBuffer* buffer= new SoundBuffer("../resources/sound/wav1.wav");
	// auto id = buffer->get_id();
	// // buffers.emplace(id, std::move(buffer));
	// AudioSource* source = new AudioSource(*this);

	// // mySpeaker->Play(sound1);
	// // mySpeaker->Play(sound2);
	// source->set_audio_buffer(id);
	// source->play();

	// std::cout << "got here\n";
}

uint32_t AudioEngine::get_buffer(const std::string& filename)
{
	auto retval = buffers.emplace(filename, filename);
	if (retval.second)
	{
		std::cout << "AudioEngine::get_buffer: new buffer created for " << filename << '\n';
	}
	return retval.first->second.get_id();
}