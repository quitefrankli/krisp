#include "SoundBuffer.hpp"
#include "utility.hpp"

#include <sndfile.h>
#include <inttypes.h>
#include <AL/alext.h>
#include <fmt/core.h>
#include <quill/LogMacros.h>

#include <string_view>
#include <stdexcept>


SoundBuffer::SoundBuffer(const std::string_view filename)
{
	ALenum err, format;
	SNDFILE* sndfile;
	SF_INFO sfinfo;
	short* membuf;
	sf_count_t num_frames;
	ALsizei num_bytes;
	/* Open the audio file and check that it's usable. */
	sndfile = sf_open(filename.data(), SFM_READ, &sfinfo);
	if (!sndfile)
	{
		throw std::runtime_error(fmt::format("Could not open audio in {}: {}\n", filename, sf_strerror(sndfile)));
	}
	if (sfinfo.frames < 1 || sfinfo.frames >(sf_count_t)(INT_MAX / sizeof(short)) / sfinfo.channels)
	{
	 	throw std::runtime_error(fmt::format("Bad sample count in {} {}\n", filename, sfinfo.frames));
	}
	if (sfinfo.channels != 1)
	{
		LOG_ERROR(Utility::get_logger(), 
		          "SoundBuffer::SoundBuffer: warning {}, channels={}, is not mono and therefore "
				  "3D sound will not be supported!", filename, sfinfo.channels);
	}

	/* Get the sound format, and figure out the OpenAL format */
	format = AL_NONE;
	if (sfinfo.channels == 1)
	{
		format = AL_FORMAT_MONO16;
	} else if (sfinfo.channels == 2)
	{
		format = AL_FORMAT_STEREO16;
	} else if (sfinfo.channels == 3)
	{
		if (sf_command(sndfile, SFC_WAVEX_GET_AMBISONIC, NULL, 0) == SF_AMBISONIC_B_FORMAT)
			format = AL_FORMAT_BFORMAT2D_16;
	} else if (sfinfo.channels == 4)
	{
		if (sf_command(sndfile, SFC_WAVEX_GET_AMBISONIC, NULL, 0) == SF_AMBISONIC_B_FORMAT)
			format = AL_FORMAT_BFORMAT3D_16;
	}
	
	if (!format)
	{
		throw std::runtime_error(fmt::format("Unsupported channel count: {}\n", sfinfo.channels));
	}

	/* Decode the whole audio file to a buffer. */
	membuf = static_cast<short*>(malloc((size_t)(sfinfo.frames * sfinfo.channels) * sizeof(short)));

	num_frames = sf_readf_short(sndfile, membuf, sfinfo.frames);
	if (num_frames < 1)
	{
		throw std::runtime_error(fmt::format("Failed to read samples in {} {}\n", filename, num_frames));
	}
	num_bytes = (ALsizei)(num_frames * sfinfo.channels) * (ALsizei)sizeof(short);

	/* Buffer the audio data into a new buffer object, then free the data and
	 * close the file.
	 */
	alGenBuffers(1, &buffer);
	alBufferData(buffer, format, membuf, num_bytes, sfinfo.samplerate);

	free(membuf);
	sf_close(sndfile);

	/* Check if an error occured, and clean up if so. */
	err = alGetError();
	if (err != AL_NO_ERROR)
	{
		throw std::runtime_error(fmt::format("OpenAL Error: {}\n", alGetString(err)));
	}
}

SoundBuffer::SoundBuffer(SoundBuffer&& other) noexcept :
	buffer(other.buffer)
{
}

SoundBuffer::~SoundBuffer()
{
	alDeleteBuffers(1, &buffer);
}
