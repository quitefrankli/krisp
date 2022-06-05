#pragma once

#if _WIN32
	#include <al.h>
	#include <alc.h>
#elif __APPLE__
	#include <OpenAL/al.h>
	#include <OpenAL/alc.h>
#else
	#error UNSUPPORTED OS!
#endif