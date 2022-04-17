#include "listener.hpp"
#include "audio_engine_pimpl.hpp"

#include <AL/al.h>


Listener::Listener(AudioEnginePimpl& engine)
{
	alListener3f(AL_POSITION, 0.0f, 0.0f, 0.0f);
	alListener3f(AL_VELOCITY, 0.0f, 0.0f, 0.0f);
	// alListener3f(AL_ORIENTATION, 0.0f, 1.0f, 0.0f);
}