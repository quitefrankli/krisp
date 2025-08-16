#pragma once

#include "constants.hpp"

#include <vulkan/vulkan_core.h>

#include <cstdint>


namespace CSTS // short for Constants
{
	// The default msaa used throughout
	const VkSampleCountFlagBits MSAA_SAMPLE_COUNT = VK_SAMPLE_COUNT_4_BIT;
};
