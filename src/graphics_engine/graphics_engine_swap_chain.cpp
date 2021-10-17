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
	swap_chain_image_format = surface_format.format;
	VkPresentModeKHR present_mode = choose_swap_present_mode(swap_chain_support.presentModes);
	swap_chain_extent = choose_swap_extent(swap_chain_support.capabilities);

	create_render_pass();

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
	if (frames.capacity() < swap_chain_images.size())
	{
		throw std::runtime_error("frame resize attempted");
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
	// 	vkDestroyFramebuffer(get_logical_device(), frame_buffer, nullptr);
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
	
	vkDestroyRenderPass(get_logical_device(), render_pass, nullptr);
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
		if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
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

	// // 1. acquire image from swap chain
	// // 2. execute command buffer with image as attachment in the frame buffer
	// // 3. return the image to swap chain for presentation

	// // CPU-GPU synchronisation for in flight images
	// vkWaitForFences(get_logical_device(), 1, &in_flight_fences[current_frame], VK_TRUE, std::numeric_limits<uint64_t>::max());

	// uint32_t swap_chain_image_index;
	// // waits until there's an image available to use in the swap chain
	// VkResult result = vkAcquireNextImageKHR(get_logical_device(), 
	// 										swap_chain, 
	// 										std::numeric_limits<uint64_t>::max(), // wait time (ns)
	// 										image_available_semaphores[current_frame], 
	// 										VK_NULL_HANDLE, 
	// 										&swap_chain_image_index); // image index of the available image
	// if (result == VK_ERROR_OUT_OF_DATE_KHR) {
	// 	reset();
	// 	return;
	// } else if (image_index != swap_chain_image_index) {
	// 	throw std::runtime_error("image_index and swap_chain_image_index mismatch!");
	// } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
	// 	throw std::runtime_error("failed to acquire swap chain image!");
	// }

	// // check if a previous frame is using this image (i.e. there is a fence to wait on)
	// if (images_in_flight[image_index] != VK_NULL_HANDLE)
	// {
	// 	vkWaitForFences(get_logical_device(), 1, &images_in_flight[image_index], VK_TRUE, std::numeric_limits<uint64_t>::max());
	// }
	// // mark the image as now being in use by this frame
	// images_in_flight[image_index] = in_flight_fences[current_frame];		

	// update_uniform_buffer(image_index);

	// //
	// // submitting the command buffer
	// //

	// VkSubmitInfo submitInfo{};
	// submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	// VkSemaphore waitSemaphores[] = { image_available_semaphores[current_frame] };
	// VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
	// // here we specify which semaphore to wait on before execution begins and in which stage of the pipeline to wait
	// submitInfo.waitSemaphoreCount = 1;
	// submitInfo.pWaitSemaphores = waitSemaphores;
	// submitInfo.pWaitDstStageMask = waitStages;

	// // here we specify which command buffers to actually submit for execution
	// submitInfo.commandBufferCount = 1;
	// submitInfo.pCommandBuffers = &command_buffers[image_index];

	// // here we specify the semaphores to signal once the command buffer has finished execution
	// VkSemaphore signal_semaphores[] = { render_finished_semaphores[current_frame] };
	// submitInfo.signalSemaphoreCount = 1;
	// submitInfo.pSignalSemaphores = signal_semaphores;

	// vkResetFences(get_logical_device(), 1, &in_flight_fences[current_frame]);
	// if (vkQueueSubmit(get_graphics_engine().get_graphics_queue(), 1, &submitInfo, in_flight_fences[current_frame]) != VK_SUCCESS)
	// {
	// 	throw std::runtime_error("failed to submit draw command buffer!");
	// }

	// //
	// // Presentation
	// // this step submits the result of the swapchain to have it eventually show up on the screen
	// //

	// VkPresentInfoKHR present_info{};
	// present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	// present_info.waitSemaphoreCount = 1;
	// // the semaphore to wait on before presentation
	// present_info.pWaitSemaphores = signal_semaphores;

	// VkSwapchainKHR swap_chains[] = { swap_chain };
	// present_info.swapchainCount = 1;
	// present_info.pSwapchains = swap_chains;
	// present_info.pImageIndices = &image_index;
	// present_info.pResults = nullptr; // allows you to specify array of VkResult values to check for every individual swap chain if presentation was successful

	// current_frame = (current_frame + 1) % MAX_FRAMES_IN_FLIGHT;

	// result = vkQueuePresentKHR(get_graphics_engine().get_present_queue(), &present_info);
	// if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || frame_buffer_resized) { // recreate if we resize window
	// 	frame_buffer_resized = false;
	// 	reset();
	// } else if (result != VK_SUCCESS) {
	// 	throw std::runtime_error("failed to present swap chain image!");
	// }
}

void GraphicsEngineSwapChain::create_render_pass()
{
	VkAttachmentDescription color_attachment{};
	color_attachment.format = get_image_format();
	color_attachment.samples = VK_SAMPLE_COUNT_1_BIT; // >1 if we are doing multisampling	
	color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; // determine what to do with the data in the attachment before rendering
	color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE; // dtermine what to do with the data in the attachment after rendering
	color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; // specifies which layout the image will have before the render pass begins
	color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR; // specifies the layout to automatically transition to when the render pass finishes

	// subpasses and attachment references
	// a single render pass can consist of multiple subpasses
	VkAttachmentReference color_attachment_ref{};
	color_attachment_ref.attachment = 0; // only works since we only have 1 attachment description
	color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS; // as opposed to compute subpass
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &color_attachment_ref;
	// subpass.pInputAttachments // attachments that read from a shader
	// subpass.pResolveAttachments // attachments used for multisampling color attachments
	// subpass.pDepthStencilAttachment // attachment for depth and stencil data
	// subpass.pPreserveAttachments // attachments that are not used by this subpass, but for which the data must be preserved

	// render pass
	VkSubpassDependency dependency{};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcAccessMask = 0;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	VkRenderPassCreateInfo render_pass_create_info{};
	render_pass_create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	render_pass_create_info.attachmentCount = 1;
	render_pass_create_info.pAttachments = &color_attachment;
	render_pass_create_info.subpassCount = 1;
	render_pass_create_info.pSubpasses = &subpass;
	render_pass_create_info.dependencyCount = 1;
	render_pass_create_info.pDependencies = &dependency;

	if (vkCreateRenderPass(get_logical_device(), &render_pass_create_info, nullptr, &render_pass) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create render pass!");
	}
}