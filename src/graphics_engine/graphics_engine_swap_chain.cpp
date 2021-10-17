// swap chain can be thought of as the image buffer of old
// they give us the ability to draw images onto them and present them onto a window

#pragma once

#include "graphics_engine_swap_chain.hpp"

#include "graphics_engine.hpp"
#include "objects.hpp"

#include <algorithm>
#include <iostream>

GraphicsEngineSwapChain::GraphicsEngineSwapChain(GraphicsEngine &engine) : GraphicsEngineBaseModule(engine)
{
	SwapChainSupportDetails swap_chain_support = query_swap_chain_support(get_physical_device(), engine.get_window_surface());
	VkSurfaceFormatKHR surface_format = choose_swap_surface_format(swap_chain_support.formats);
	VkPresentModeKHR present_mode = choose_swap_present_mode(swap_chain_support.presentModes);
	swap_chain_extent = choose_swap_extent(swap_chain_support.capabilities);

	//+1 means that we won't have to wait for driver to complete internal operations before we can acquire another image to render
	unsigned image_count = swap_chain_support.capabilities.minImageCount + 1;
	if (swap_chain_support.capabilities.maxImageCount > 0) // 0 is a special number meaning there is unlimited number
	{
		image_count = std::min(image_count, swap_chain_support.capabilities.maxImageCount);
	}
	else
	{
		throw std::runtime_error("create_swap_chain: infinite swap chains!");
	}

	VkSwapchainCreateInfoKHR create_info{};
	create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	create_info.surface = get_graphics_engine().get_window_surface();
	create_info.minImageCount = image_count;
	create_info.imageFormat = surface_format.format;
	create_info.imageColorSpace = surface_format.colorSpace;
	create_info.imageExtent = swap_chain_extent;
	create_info.imageArrayLayers = 1; // number of layers per image, should always = 1
	// color attachment bit refers to rendering directly, if we want post processing we should use something like VK_IMAGE_USAGE_TRANSFER_DST_BIT
	create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	QueueFamilyIndices indices = get_graphics_engine().findQueueFamilies(get_physical_device());
	uint32_t queue_family_indices[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};
	if (indices.graphicsFamily != indices.presentFamily)
	{
		create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT; // means image is owned by one queue family at a time, ownership must be explicitly transferred
		create_info.queueFamilyIndexCount = 2;
		create_info.pQueueFamilyIndices = queue_family_indices;
	}
	else
	{
		create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE; // images can be accessed across multiple queue families without explicit ownership transfers
		create_info.queueFamilyIndexCount = 0;					  // Optional
		create_info.pQueueFamilyIndices = nullptr;				  // Optional
	}

	// images will be drawn on the swap chain in the graphics queue
	// and will be submitted in the presentation queue

	create_info.preTransform = swap_chain_support.capabilities.currentTransform; // specifies no transform
	create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;				 // useful if we want to blend with other windows in the window system, typically opaque
	create_info.presentMode = present_mode;
	create_info.clipped = true;				   // best performance (ignore pixels that are obscured by another window)
	create_info.oldSwapchain = VK_NULL_HANDLE; // assume swap chain never gets invalidated (which can happen when we resize the window)

	if (vkCreateSwapchainKHR(get_logical_device(), &create_info, nullptr, &swap_chain) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create swap chain!");
	}

	vkGetSwapchainImagesKHR(get_logical_device(), swap_chain, &image_count, nullptr); // get num images
	std::vector<VkImage> swap_chain_images(image_count);
	// frames.resize(image_count);
	vkGetSwapchainImagesKHR(get_logical_device(), swap_chain, &image_count, swap_chain_images.data());
	//GraphicsEngineFrame frame(get_graphics_engine(), *this, swap_chain_images[0]); // DELETE ME

	frames.reserve(swap_chain_images.size());
	if (frames.capacity() < swap_chain_images.size() || swap_chain_images.size() != EXPECTED_NUM_SWAPCHAIN_IMAGES)
	{
		throw std::runtime_error("GraphicsEngineSwapChain:: ERROR in num swapchain images!");
	}
	for (auto &handle : swap_chain_images)
	{
		std::cout << handle << '\n';
		// create the frames
		frames.emplace_back(get_graphics_engine(), *this, handle);
	}

	std::cout << "swap chain created, count=" << image_count << std::endl;
}

GraphicsEngineSwapChain::~GraphicsEngineSwapChain()
{
	// for (auto& frame_buffer : swap_chain_frame_buffers)
	// {
	// 	vkDestroyFramebuffer(get_logical_device(), frame_buffer, nullptr); // moved this to frame
	// }

	// vkFreeCommandBuffers(get_logical_device(), command_pool, static_cast<uint32_t>(command_buffers.size()), command_buffers.data()); // moved frame destructor

	// for (auto& swap_chain_image : swap_chain_image_views)
	// {
	// 	vkDestroyImageView(get_logical_device(), swap_chain_image, nullptr);
	// }

	vkDestroySwapchainKHR(get_logical_device(), swap_chain, nullptr);

	// for (size_t i = 0; i < swap_chain_images.size(); i++)
	// {
	// 	vkDestroyBuffer(get_logical_device(), uniform_buffers[i], nullptr);
	// 	vkFreeMemory(get_logical_device(), uniform_buffers_memory[i], nullptr);
	// }

	// vkDestroyDescriptorPool(get_logical_device(), descriptor_pool, nullptr);

	// destroy synchronisation object
	//for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	//{
	//	vkDestroySemaphore(get_logical_device(), image_available_semaphores[i], nullptr);
	//	vkDestroySemaphore(get_logical_device(), render_finished_semaphores[i], nullptr);
	//	vkDestroyFence(get_logical_device(), in_flight_fences[i], nullptr);
	//}
}

void GraphicsEngineSwapChain::reset()
{
	auto &engine = get_graphics_engine();
	this->~GraphicsEngineSwapChain();
	new (this) GraphicsEngineSwapChain(engine);
}

SwapChainSupportDetails GraphicsEngineSwapChain::query_swap_chain_support(VkPhysicalDevice& device, VkSurfaceKHR& surface)
{
	SwapChainSupportDetails details;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

	uint32_t format_count;
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &format_count, nullptr);

	if (format_count)
	{
		details.formats.resize(format_count);
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &format_count, details.formats.data());
	}

	uint32_t present_mode_count;
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &present_mode_count, nullptr);

	if (present_mode_count)
	{
		details.presentModes.resize(present_mode_count);
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &present_mode_count, details.presentModes.data());
	}

	return details;
}

VkSurfaceFormatKHR GraphicsEngineSwapChain::choose_swap_surface_format(const std::vector<VkSurfaceFormatKHR> &available_formats)
{
	for (const auto &format : available_formats)
	{
		if (format.format == get_image_format() && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
		{
			return format;
		}
	}

	return available_formats[0]; // the first format is usually good enough
}

VkPresentModeKHR GraphicsEngineSwapChain::choose_swap_present_mode(const std::vector<VkPresentModeKHR> &available_present_modes)
{
	for (const auto &mode : available_present_modes)
	{
		if (mode == VK_PRESENT_MODE_MAILBOX_KHR)
		{
			return mode;
		}
	}

	return VK_PRESENT_MODE_FIFO_KHR; // guranteed
}

// extent = resolution of the swap chain images and ~ resolution of window we are drawing to
VkExtent2D GraphicsEngineSwapChain::choose_swap_extent(const VkSurfaceCapabilitiesKHR &capabilities)
{
	if (capabilities.currentExtent.width != UINT32_MAX)
	{
		return capabilities.currentExtent;
	}
	else
	{
		int width, height;
		glfwGetFramebufferSize(get_graphics_engine().get_window(), &width, &height);
		VkExtent2D actual_extent = {
			static_cast<uint32_t>(width),
			static_cast<uint32_t>(height)};

		actual_extent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, actual_extent.width));
		actual_extent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, actual_extent.height));

		return actual_extent;
	}
}

// void GraphicsEngineSwapChain::recreate_swap_chain()
// {
// 	// clean_up_swap_chain(); // moved to destructor

// 	// create_swap_chain();
// 	// create_image_views();
// 	// create_render_pass();
// 	// create_graphics_pipeline();
// 	// create_frame_buffers();
// 	// create_uniform_buffers(); // this one should be frame specific... we will work out details later
// 	// create_descriptor_pools();
// 	// create_descriptor_sets(); // moved to on object spawn basis
// 	// 1 by 1 to recreate the descriptor sets appropriately
// 	// create_command_buffers(); // moved to on object spawn basis
// }

void GraphicsEngineSwapChain::spawn_object(GraphicsEngineObject& object)
{
	for (auto &frame : frames)
	{
		frame.spawn_object(object);
	}
}

void GraphicsEngineSwapChain::draw()
{
	frames[current_frame].draw();

	current_frame = (current_frame + 1) % frames.size();
}