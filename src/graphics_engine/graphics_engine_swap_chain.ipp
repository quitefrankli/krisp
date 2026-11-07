// Swap chain images are owned by the presentation engine. This module owns their
// views, per-image frame state, and the synchronization used to retire them.

#pragma once

#include "graphics_engine_swap_chain.hpp"

#include "graphics_engine.hpp"
#include "objects/object.hpp"
#include "utility.hpp"

#include <quill/LogMacros.h>

#include <algorithm>
#include <iostream>
#include <limits>


GraphicsEngineSwapChain::GraphicsEngineSwapChain(GraphicsEngine& engine) : GraphicsEngineBaseModule(engine)
{
	const auto window_state = engine.get_window().get_presentation_state();
	const VkExtent2D framebuffer_extent{
		static_cast<uint32_t>(std::max(window_state.framebuffer_width, 0)),
		static_cast<uint32_t>(std::max(window_state.framebuffer_height, 0))};
	if (framebuffer_extent.width == 0 || framebuffer_extent.height == 0)
	{
		extent = {};
		engine.set_extent(extent);
		return;
	}

	swap_chain = create_swap_chain(framebuffer_extent, VK_NULL_HANDLE);
	if (swap_chain == VK_NULL_HANDLE)
		return;
	try
	{
		const auto images = get_swap_chain_images(swap_chain);
		destroy_and_recreate_frames(images);
	}
	catch (...)
	{
		engine.get_renderer_mgr().release_per_frame_resources(true);
		for (const auto& slot : acquisition_slots)
			if (slot.image_available != VK_NULL_HANDLE)
				vkDestroySemaphore(get_logical_device(), slot.image_available, nullptr);
		acquisition_slots.clear();
		frames.clear();
		if (swap_chain != VK_NULL_HANDLE)
			vkDestroySwapchainKHR(get_logical_device(), swap_chain, nullptr);
		swap_chain = VK_NULL_HANDLE;
		throw;
	}
}

GraphicsEngineSwapChain::~GraphicsEngineSwapChain()
{
	try
	{
		for (auto& frame : frames) frame.wait_for_completion();
	}
	catch (...)
	{
		// Destructors must still release Vulkan objects after a device error.
	}
	for (const auto& slot : acquisition_slots)
		if (slot.image_available != VK_NULL_HANDLE)
			vkDestroySemaphore(get_logical_device(), slot.image_available, nullptr);
	acquisition_slots.clear();
	frames.clear();
	if (swap_chain != VK_NULL_HANDLE)
		vkDestroySwapchainKHR(get_logical_device(), swap_chain, nullptr);
}

VkSwapchainKHR GraphicsEngineSwapChain::create_swap_chain(
	VkExtent2D framebuffer_extent, VkSwapchainKHR old_swap_chain)
{
	const auto support = query_swap_chain_support(get_physical_device(), get_graphics_engine().get_window_surface());
	const auto surface_format = choose_swap_surface_format(support.formats);
	const auto new_extent = choose_swap_extent(support.capabilities, framebuffer_extent);
	const auto new_present_mode = choose_swap_present_mode(support.presentModes);
	if (new_extent.width == 0 || new_extent.height == 0)
		return VK_NULL_HANDLE;

	uint32_t image_count = std::max(CSTS::DESIRED_SWAPCHAIN_IMAGES, support.capabilities.minImageCount);
	if (support.capabilities.maxImageCount != 0)
		image_count = std::min(image_count, support.capabilities.maxImageCount);
	if (image_count > CSTS::UPPERBOUND_SWAPCHAIN_IMAGES)
		throw std::runtime_error("surface requires more swap chain images than the engine supports");

	VkSwapchainCreateInfoKHR create_info{};
	create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	create_info.surface = get_graphics_engine().get_window_surface();
	create_info.minImageCount = image_count;
	create_info.imageFormat = surface_format.format;
	create_info.imageColorSpace = surface_format.colorSpace;
	create_info.imageExtent = new_extent;
	create_info.imageArrayLayers = 1;
	create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
		VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

	const auto indices = get_graphics_engine().findQueueFamilies(get_physical_device());
	const uint32_t queue_family_indices[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};
	if (indices.graphicsFamily != indices.presentFamily)
	{
		create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		create_info.queueFamilyIndexCount = 2;
		create_info.pQueueFamilyIndices = queue_family_indices;
	}
	else
		create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;

	create_info.preTransform = support.capabilities.currentTransform;
	create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	create_info.presentMode = new_present_mode;
	create_info.clipped = VK_TRUE;
	create_info.oldSwapchain = old_swap_chain;

	VkSwapchainKHR created = VK_NULL_HANDLE;
	if (vkCreateSwapchainKHR(get_logical_device(), &create_info, nullptr, &created) != VK_SUCCESS)
		throw std::runtime_error("failed to create swap chain");

	extent = new_extent;
	image_format = surface_format.format;
	color_space = surface_format.colorSpace;
	present_mode = new_present_mode;
	get_graphics_engine().set_extent(extent);
	return created;
}

std::vector<VkImage> GraphicsEngineSwapChain::get_swap_chain_images(VkSwapchainKHR target)
{
	uint32_t image_count = 0;
	if (vkGetSwapchainImagesKHR(get_logical_device(), target, &image_count, nullptr) != VK_SUCCESS || image_count == 0)
		throw std::runtime_error("failed to query swap chain images");
	if (image_count > CSTS::UPPERBOUND_SWAPCHAIN_IMAGES)
		throw std::runtime_error(
			"surface returned more swap chain images than the engine's per-image resource capacity supports");
	std::vector<VkImage> images(image_count);
	if (vkGetSwapchainImagesKHR(get_logical_device(), target, &image_count, images.data()) != VK_SUCCESS)
		throw std::runtime_error("failed to retrieve swap chain images");
	images.resize(image_count);
	return images;
}

void GraphicsEngineSwapChain::recreate(VkExtent2D framebuffer_extent)
{
	if (framebuffer_extent.width == 0 || framebuffer_extent.height == 0)
		return;

	const auto old_swap_chain = swap_chain;
	const auto replacement = create_swap_chain(framebuffer_extent, old_swap_chain);
	if (replacement == VK_NULL_HANDLE)
		return;
	std::vector<VkImage> images;
	try
	{
		images = get_swap_chain_images(replacement);
	}
	catch (...)
	{
		vkDestroySwapchainKHR(get_logical_device(), replacement, nullptr);
		throw;
	}

	const bool image_count_changed = images.size() != frames.size();
	auto& engine = get_graphics_engine();
	engine.get_graphics_gui_manager().release_per_frame_resources(image_count_changed);
	engine.get_renderer_mgr().release_per_frame_resources(image_count_changed);

	for (const auto& slot : acquisition_slots)
		if (slot.image_available != VK_NULL_HANDLE)
			vkDestroySemaphore(get_logical_device(), slot.image_available, nullptr);
	acquisition_slots.clear();
	frames.clear();
	if (old_swap_chain != VK_NULL_HANDLE)
		vkDestroySwapchainKHR(get_logical_device(), old_swap_chain, nullptr);
	swap_chain = replacement;

	try
	{
		destroy_and_recreate_frames(images);
		engine.get_graphics_gui_manager().rebuild_per_frame_resources(image_count_changed);
	}
	catch (...)
	{
		engine.get_renderer_mgr().release_per_frame_resources(true);
		engine.get_graphics_gui_manager().release_per_frame_resources(true);
		frames.clear();
		for (const auto& slot : acquisition_slots)
			if (slot.image_available != VK_NULL_HANDLE)
				vkDestroySemaphore(get_logical_device(), slot.image_available, nullptr);
		acquisition_slots.clear();
		vkDestroySwapchainKHR(get_logical_device(), swap_chain, nullptr);
		swap_chain = VK_NULL_HANDLE;
		extent = {};
		engine.set_extent(extent);
		throw;
	}
}

void GraphicsEngineSwapChain::drain()
{
	std::exception_ptr failure;
	for (auto& frame : frames)
	{
		try { frame.drain(); }
		catch (...) { if (!failure) failure = std::current_exception(); }
	}
	if (failure) std::rethrow_exception(failure);
}

VkSurfaceFormatKHR GraphicsEngineSwapChain::choose_swap_surface_format(
	const std::vector<VkSurfaceFormatKHR>& available_formats)
{
	constexpr VkFormat required_format = VK_FORMAT_B8G8R8A8_SRGB;
	constexpr VkColorSpaceKHR required_color_space = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
	if (available_formats.size() == 1 && available_formats.front().format == VK_FORMAT_UNDEFINED
		&& available_formats.front().colorSpace == required_color_space)
		return {required_format, required_color_space};
	for (const auto& format : available_formats)
		if (format.format == required_format && format.colorSpace == required_color_space)
			return format;
	throw std::runtime_error("surface must support B8G8R8A8_SRGB with SRGB_NONLINEAR color space");
}

VkPresentModeKHR GraphicsEngineSwapChain::choose_swap_present_mode(
	const std::vector<VkPresentModeKHR>& available_present_modes)
{
	if (get_graphics_engine().get_runtime_options().presentation == PresentationPreference::Uncapped
		&& std::find(available_present_modes.begin(), available_present_modes.end(), VK_PRESENT_MODE_IMMEDIATE_KHR)
			!= available_present_modes.end())
		return VK_PRESENT_MODE_IMMEDIATE_KHR;
	if (std::find(available_present_modes.begin(), available_present_modes.end(), VK_PRESENT_MODE_MAILBOX_KHR)
		!= available_present_modes.end())
		return VK_PRESENT_MODE_MAILBOX_KHR;
	return VK_PRESENT_MODE_FIFO_KHR;
}

SwapChainDrawOutcome GraphicsEngineSwapChain::draw()
{
	if (swap_chain == VK_NULL_HANDLE || frames.empty())
		return {.presented = false, .recreate = true};

	auto& engine = get_graphics_engine();
	const auto& observation = engine.get_runtime_options().observation;
	const bool collect_cpu = observation && observation->active();
	const auto observation_submission = observation ? engine.next_observation_submission() : 0;
	const auto& render_frame = engine.get_render_frame();
	const auto record_cpu_sample = [&](RuntimeSampleKind kind, std::uint64_t begin_ns,
		std::uint64_t end_ns, bool success)
	{
		if (collect_cpu)
			observation->record_graphics(RuntimeSample{
				.kind = kind, .begin_ns = begin_ns, .end_ns = end_ns,
				.submission_id = observation_submission, .frame_number = render_frame.frame_number,
				.tick_index = render_frame.simulation_tick, .publication_ns = render_frame.publication_ns,
				.success = success});
	};

	auto& slot = acquisition_slots[current_acquisition_slot];
	if (slot.last_submission != VK_NULL_HANDLE)
	{
		const auto begin_ns = collect_cpu ? runtime_now_ns() : 0;
		const auto result = vkWaitForFences(get_logical_device(), 1, &slot.last_submission,
			VK_TRUE, std::numeric_limits<std::uint64_t>::max());
		record_cpu_sample(RuntimeSampleKind::FrameFenceWait, begin_ns,
			collect_cpu ? runtime_now_ns() : 0, result == VK_SUCCESS);
		if (result != VK_SUCCESS)
			throw std::runtime_error("failed to wait for acquisition semaphore reuse");
	}

	uint32_t image_index = 0;
	const auto acquire_begin_ns = collect_cpu ? runtime_now_ns() : 0;
	const auto acquire_result = vkAcquireNextImageKHR(get_logical_device(), swap_chain,
		std::numeric_limits<std::uint64_t>::max(), slot.image_available, VK_NULL_HANDLE, &image_index);
	const bool acquired = acquire_result == VK_SUCCESS || acquire_result == VK_SUBOPTIMAL_KHR;
	record_cpu_sample(RuntimeSampleKind::Acquire, acquire_begin_ns,
		collect_cpu ? runtime_now_ns() : 0, acquired);
	if (acquire_result == VK_ERROR_OUT_OF_DATE_KHR)
		return {.presented = false, .recreate = true};
	if (!acquired)
		throw std::runtime_error("failed to acquire swapchain image");

	auto& frame = frames.at(image_index);
	const VkResult present_result = frame.draw(slot.image_available, observation_submission);
	slot.last_submission = frame.get_submission_fence();
	current_acquisition_slot = (current_acquisition_slot + 1) % acquisition_slots.size();
	if (present_result != VK_SUCCESS && present_result != VK_SUBOPTIMAL_KHR && present_result != VK_ERROR_OUT_OF_DATE_KHR)
		throw std::runtime_error("failed to present swap chain image");
	const bool successful_present = present_result == VK_SUCCESS || present_result == VK_SUBOPTIMAL_KHR;
	const bool needs_recreate = acquire_result == VK_SUBOPTIMAL_KHR
		|| present_result == VK_SUBOPTIMAL_KHR || present_result == VK_ERROR_OUT_OF_DATE_KHR;
	return {.presented = successful_present, .recreate = needs_recreate};
}

void GraphicsEngineSwapChain::destroy_and_recreate_frames(const std::vector<VkImage>& images)
{
	current_acquisition_slot = 0;
	frames.reserve(images.size());
	for (uint32_t image_index = 0; image_index < images.size(); ++image_index)
		frames.emplace_back(get_graphics_engine(), *this, images[image_index], image_index);

	acquisition_slots.resize(images.size());
	VkSemaphoreCreateInfo semaphore_info{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
	for (auto& slot : acquisition_slots)
	{
		if (vkCreateSemaphore(get_logical_device(), &semaphore_info, nullptr, &slot.image_available) != VK_SUCCESS)
			throw std::runtime_error("failed to create acquisition semaphore");
	}
	LOG_INFO(Utility::get_logger(), "GraphicsEngineSwapChain: created {} frames", frames.size());
}

SwapChainSupportDetails GraphicsEngineSwapChain::query_swap_chain_support(
	VkPhysicalDevice physical_device, VkSurfaceKHR window_surface)
{
	SwapChainSupportDetails details;
	if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, window_surface, &details.capabilities) != VK_SUCCESS)
		throw std::runtime_error("failed to query surface capabilities");
	uint32_t format_count = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, window_surface, &format_count, nullptr);
	if (format_count)
	{
		details.formats.resize(format_count);
		vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, window_surface, &format_count, details.formats.data());
		details.formats.resize(format_count);
	}
	uint32_t present_mode_count = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, window_surface, &present_mode_count, nullptr);
	if (present_mode_count)
	{
		details.presentModes.resize(present_mode_count);
		vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, window_surface, &present_mode_count, details.presentModes.data());
		details.presentModes.resize(present_mode_count);
	}
	return details;
}

VkExtent2D GraphicsEngineSwapChain::choose_swap_extent(
	const VkSurfaceCapabilitiesKHR& capabilities, VkExtent2D framebuffer_extent)
{
	if (capabilities.currentExtent.width != UINT32_MAX)
		return capabilities.currentExtent;
	return {
		std::clamp(framebuffer_extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
		std::clamp(framebuffer_extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height)};
}
