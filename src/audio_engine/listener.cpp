#include "listener.hpp"
#include "audio_engine_pimpl.hpp"

#include <AL/al.h>


void Listener::set_pos(const glm::vec3& pos)
{
	alListener3f(AL_POSITION, ALfloat(pos.x), ALfloat(pos.y), ALfloat(pos.z));
	alListener3f(AL_VELOCITY, 0.0f, 0.0f, 0.0f);
	// alListener3f(AL_ORIENTATION, 0.0f, 1.0f, 0.0f);
}
