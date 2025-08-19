#pragma once

#include <cstdint>


namespace CSTS // short for Constants
{
	// Used for various initialisations and buffers
	// smaller values improve memory and performance
	// The actual value is only known at runtime, if the actual value exeeds the upperbound
	// the program will throw and this value would need to be increased
	constexpr uint32_t UPPERBOUND_SWAPCHAIN_IMAGES = 4;
	
	// Driver may return a number greater than this value
	// in such an event the program will adust to to use provided value
	constexpr uint32_t DESIRED_SWAPCHAIN_IMAGES = 3;
	static_assert(DESIRED_SWAPCHAIN_IMAGES <= UPPERBOUND_SWAPCHAIN_IMAGES);

	// The default msaa used throughout
	const uint32_t MSAA_SAMPLE_COUNT = 4;	
};
