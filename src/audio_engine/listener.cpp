#include "listener.hpp"

#include "audio_engine_pimpl.hpp"

void Listener::set_transform(
	const glm::vec3& position,
	const glm::vec3& direction,
	const glm::vec3& up)
{
	if (audio_engine)
		audio_engine->set_listener_transform(position, direction, up);
}
