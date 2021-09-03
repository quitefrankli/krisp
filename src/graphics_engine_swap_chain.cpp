// swap chain can be thought of as the image buffer of old
// they give us the ability to draw images onto them and present them onto a window

#pragma once

#include <algorithm>

#include "graphics_engine.hpp"

void GraphicsEngine::create_swap_chain()
{
	SwapChainSupportDetails swap_chain_support = query_swap_chain_support(physicalDevice);
	VkSurfaceFormatKHR surface_format = choose_swap_surface_format(swap_chain_support.formats);
	VkPresentModeKHR present_mode = choose_swap_present_mode(swap_chain_support.presentModes);
	VkExtent2D extent = choose_swap_extent(swap_chain_support.capabilities);

	//+1 means that we won't have to wait for driver to complete internal operations before we can acquire another image to render
	unsigned image_count = swap_chain_support.capabilities.minImageCount + 1; 
	if (swap_chain_support.capabilities.maxImageCount > 0) // 0 is a special number meaning there is unlimited number
	{
		image_count = std::min(image_count, swap_chain_support.capabilities.maxImageCount);
	}	

	VkSwapchainCreateInfoKHR create_info{};
	create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	create_info.surface = window_surface;
	create_info.minImageCount = image_count;
	create_info.imageFormat = surface_format.format;
	create_info.imageColorSpace = surface_format.colorSpace;
	create_info.imageExtent = extent;
	create_info.imageArrayLayers = 1; // number of layers per image, should always = 1
	// color attachment bit refers to rendering directly, if we want post processing we should use something like VK_IMAGE_USAGE_TRANSFER_DST_BIT
	create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
	uint32_t queue_family_indices[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };
	if (indices.graphicsFamily != indices.presentFamily) {
		create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT; // means image is owned by one queue family at a time, ownership must be explicitly transferred
		create_info.queueFamilyIndexCount = 2;
		create_info.pQueueFamilyIndices = queue_family_indices;
	} else {
		create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE; // images can be accessed across multiple queue families without explicit ownership transfers
		create_info.queueFamilyIndexCount = 0; // Optional
		create_info.pQueueFamilyIndices = nullptr; // Optional
	}

	// images will be drawn on the swap chain in the graphics queue
	// and will be submitted in the presentation queue

	create_info.preTransform = swap_chain_support.capabilities.currentTransform; // specifies no transform
	create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR; // useful if we want to blend with other windows in the window system, typically opaque
	create_info.presentMode = present_mode;
	create_info.clipped = true;  // best performance (ignore pixels that are obscured by another window)
	create_info.oldSwapchain = VK_NULL_HANDLE; // assume swap chain never gets invalidated (which can happen when we resize the window)

	if (vkCreateSwapchainKHR(logical_device, &create_info, nullptr, &swap_chain) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create swap chain!");
	}

	vkGetSwapchainImagesKHR(logical_device, swap_chain, &image_count, nullptr);
	swap_chain_images.resize(image_count);
	vkGetSwapchainImagesKHR(logical_device, swap_chain, &image_count, swap_chain_images.data());

	swap_chain_image_format = surface_format.format;
	swap_chain_extent = extent;

	std::cout << "swap chain created\n";
}

SwapChainSupportDetails GraphicsEngine::query_swap_chain_support(VkPhysicalDevice device)
{
	SwapChainSupportDetails details;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, window_surface, &details.capabilities);

	uint32_t format_count;
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, window_surface, &format_count, nullptr);

	if (format_count)
	{
		details.formats.resize(format_count);
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, window_surface, &format_count, details.formats.data());
	}

	uint32_t present_mode_count;
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, window_surface, &present_mode_count, nullptr);

	if (present_mode_count)
	{
		details.presentModes.resize(present_mode_count);
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, window_surface, &present_mode_count, details.presentModes.data());
	}

	return details;
}

VkSurfaceFormatKHR GraphicsEngine::choose_swap_surface_format(const std::vector<VkSurfaceFormatKHR>& available_formats)
{
	for (const auto& format : available_formats)
	{
		if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
		{
			return format;
		}
	}

	return available_formats[0]; // the first format is usually good enough
}

VkPresentModeKHR GraphicsEngine::choose_swap_present_mode(const std::vector<VkPresentModeKHR>& available_present_modes)
{
	for (const auto& mode : available_present_modes)
	{
		if (mode == VK_PRESENT_MODE_MAILBOX_KHR)
		{
			return mode;
		}
	}

	return VK_PRESENT_MODE_FIFO_KHR; // guranteed
}

// extent = resolution of the swap chain images and ~ resolution of window we are drawing to
VkExtent2D GraphicsEngine::choose_swap_extent(const VkSurfaceCapabilitiesKHR& capabilities)
{
	if (capabilities.currentExtent.width != UINT32_MAX)
	{
		return capabilities.currentExtent;
	} else 
	{
		int width, height;
		glfwGetFramebufferSize(window, &width, &height);
		VkExtent2D actual_extent = {
			static_cast<uint32_t>(width),
			static_cast<uint32_t>(height)
		};

		actual_extent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, actual_extent.width));
		actual_extent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, actual_extent.height));

		return actual_extent;
	}
}

void GraphicsEngine::recreate_swap_chain()
{
	// for when window is minimised
	int width = 0, height = 0;
    glfwGetFramebufferSize(window, &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(window, &width, &height);
        glfwWaitEvents();
    }

	vkDeviceWaitIdle(logical_device); // we want to wait until resource is no longer in use

	clean_up_swap_chain();

	create_swap_chain();
	create_image_views();
	create_render_pass();
	create_graphics_pipeline();
	create_frame_buffers();
	create_uniform_buffers();
	create_descriptor_pool();
	create_descriptor_sets();
	create_command_buffers();
}

void GraphicsEngine::clean_up_swap_chain()
{
	for (auto& frame_buffer : swap_chain_frame_buffers)
	{
		vkDestroyFramebuffer(logical_device, frame_buffer, nullptr);
	}

	vkFreeCommandBuffers(logical_device, command_pool, static_cast<uint32_t>(command_buffers.size()), command_buffers.data());
	vkDestroyPipeline(logical_device, graphics_engine_pipeline, nullptr);
	vkDestroyPipelineLayout(logical_device, pipeline_layout, nullptr);
	vkDestroyRenderPass(logical_device, render_pass, nullptr);

	for (auto& swap_chain_image : swap_chain_image_views)
	{
		vkDestroyImageView(logical_device, swap_chain_image, nullptr);
	}

	vkDestroySwapchainKHR(logical_device, swap_chain, nullptr);

	for (size_t i = 0; i < swap_chain_images.size(); i++)
	{
		vkDestroyBuffer(logical_device, uniform_buffers[i], nullptr);
		vkFreeMemory(logical_device, uniform_buffers_memory[i], nullptr);
	}

	vkDestroyDescriptorPool(logical_device, descriptor_pool, nullptr);
}

void GraphicsEngine::create_synchronisation_objects()
{
	image_available_semaphores.resize(MAX_FRAMES_IN_FLIGHT);
	render_finished_semaphores.resize(MAX_FRAMES_IN_FLIGHT);
	in_flight_fences.resize(MAX_FRAMES_IN_FLIGHT);
	images_in_flight.resize(swap_chain_images.size(), VK_NULL_HANDLE);

	VkSemaphoreCreateInfo semaphore_create_info{};
	semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo fence_create_info{};
	fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fence_create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		if (vkCreateSemaphore(logical_device, &semaphore_create_info, nullptr, &image_available_semaphores[i]) != VK_SUCCESS ||
			vkCreateSemaphore(logical_device, &semaphore_create_info, nullptr, &render_finished_semaphores[i]) != VK_SUCCESS ||
			vkCreateFence(logical_device, &fence_create_info, nullptr, &in_flight_fences[i]) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create semaphores!");
		}
	}
}