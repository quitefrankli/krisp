#pragma once

#include <vulkan/vulkan_core.h>

#include <cstdint>


namespace CSTS // short for Constants
{
	const uint32_t NUM_EXPECTED_SWAPCHAIN_IMAGES = 3;

	// The default msaa used throughout
	const VkSampleCountFlagBits MSAA_SAMPLE_COUNT = VK_SAMPLE_COUNT_4_BIT;
};
