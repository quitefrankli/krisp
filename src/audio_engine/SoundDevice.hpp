#pragma once

#include <AL\alc.h>
#include <AL\al.h>

#include <stdio.h>
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
		printf("Opened \"%s\"\n", name);
	}

	~SoundDevice()
	{
		if (!alcMakeContextCurrent(nullptr))
			throw("failed to set context to nullptr");

		alcDestroyContext(p_ALCContext);
		ALenum err = alcGetError(p_ALCDevice);
    	if (err != AL_NO_ERROR)
        	std::cerr << "[ERROR]::alcDestroyContext() error: " << alcGetString(p_ALCDevice, err) << std::endl;
		if (p_ALCContext)
			// throw std::runtime_error("failed to unset during close"); // for some reason the above doesn't catch the error so can't really debug for now
			std::cerr << "SoundDevice::~SoundDevice(): failed to unset context during close\n";

		if (!alcCloseDevice(p_ALCDevice))
			throw("failed to close sound device");
	}

	ALCdevice* p_ALCDevice;
	ALCcontext* p_ALCContext;
};