#pragma once

#include "openal_include.hpp"
#include "utility.hpp"

#include <quill/Quill.h>

#include <iostream>


class SoundDevice
{
public:
	SoundDevice()
	{
		p_ALCDevice = alcOpenDevice(nullptr); // nullptr = get default device
		if (!p_ALCDevice)
			throw("failed to get sound device");

		p_ALCContext = alcCreateContext(p_ALCDevice, nullptr);  // create context
		if(!p_ALCContext)
			throw("Failed to set sound context");

		if (!alcMakeContextCurrent(p_ALCContext))   // make context current
			throw("failed to make context current");

		const ALCchar* name = nullptr;
		if (alcIsExtensionPresent(p_ALCDevice, "ALC_ENUMERATE_ALL_EXT"))
			name = alcGetString(p_ALCDevice, ALC_ALL_DEVICES_SPECIFIER);
		if (!name || alcGetError(p_ALCDevice) != AL_NO_ERROR)
			name = alcGetString(p_ALCDevice, ALC_DEVICE_SPECIFIER);

		LOG_INFO(Utility::get_logger(), "SoundDevice::SoundDevice: Opened {}", name);
	}

	~SoundDevice()
	{
		if (!alcMakeContextCurrent(nullptr))
		{
			LOG_ERROR(Utility::get_logger(), "SoundDevice: failed to set context to nullptr");
			return;
		}

		alcDestroyContext(p_ALCContext);
		ALenum err = alcGetError(p_ALCDevice);
    	if (err != AL_NO_ERROR)
		{
			LOG_ERROR(Utility::get_logger(), "SoundDevice: ERROR with alcDestroyContext: {}", alcGetString(p_ALCDevice, err));
			return;
		}

		if (!alcCloseDevice(p_ALCDevice))
		{
			LOG_ERROR(Utility::get_logger(), "SoundDevice: failed close");
			return;
		}
	}

	ALCdevice* p_ALCDevice;
	ALCcontext* p_ALCContext;
};