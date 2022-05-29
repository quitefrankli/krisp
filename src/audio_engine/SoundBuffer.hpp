#pragma once

#include "openal_include.hpp"

#include <string_view>


class SoundBuffer
{
public:
	SoundBuffer(const std::string_view filename);
	SoundBuffer(SoundBuffer&& other) noexcept;

	ALuint get_id() const
	{
		return buffer;
	}

	~SoundBuffer();

private:
	ALuint buffer = 0;
};

