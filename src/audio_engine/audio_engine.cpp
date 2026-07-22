#include <extras/stb_vorbis.c>
#undef L
#undef C
#undef R
#define MINIAUDIO_IMPLEMENTATION
#include "audio_engine.hpp"

#include "utility.hpp"

#include <quill/LogMacros.h>

#include <stdexcept>

namespace
{
std::runtime_error audio_error(const char* operation, const ma_result result)
{
	return std::runtime_error(std::string(operation) + ": " + ma_result_description(result));
}
}

AudioEngine::AudioEngine(const bool enable_output)
{
	ma_result result = MA_ERROR;
	if (enable_output)
	{
		result = ma_engine_init(nullptr, &engine);
		if (result == MA_SUCCESS)
		{
			initialized = true;
			const ma_device* device = ma_engine_get_device(&engine);
			output_available = device && device->pContext
				&& device->pContext->backend != ma_backend_null;
			if (!output_available)
				LOG_WARNING(Utility::get_logger(),
					"No physical audio output is available; using miniaudio's null backend");
			return;
		}
		LOG_WARNING(Utility::get_logger(),
			"Audio output unavailable ({}); continuing without a device",
			ma_result_description(result));
	}

	auto config = ma_engine_config_init();
	config.noDevice = MA_TRUE;
	config.channels = 2;
	config.sampleRate = 48000;
	result = ma_engine_init(&config, &engine);
	if (result != MA_SUCCESS)
		throw audio_error("Failed to initialize miniaudio", result);
	initialized = true;
}

AudioEngine::~AudioEngine()
{
	if (initialized)
		ma_engine_uninit(&engine);
}

void AudioEngine::set_listener_transform(
	const glm::vec3& position,
	const glm::vec3& direction,
	const glm::vec3& up)
{
	ma_engine_listener_set_position(&engine, 0, position.x, position.y, position.z);
	ma_engine_listener_set_direction(&engine, 0, direction.x, direction.y, direction.z);
	ma_engine_listener_set_world_up(&engine, 0, up.x, up.y, up.z);
}
