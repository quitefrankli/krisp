#pragma once

#include <vulkan/vulkan.hpp>


struct GraphicsBuffer
{
	VkBuffer buffer;
	VkDeviceMemory memory;
};