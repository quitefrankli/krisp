// swap chain can be thought of as the image buffer of old
// they give us the ability to draw images onto them and present them onto a window

#pragma once

#include "graphics_engine_swap_chain.hpp"

#include "graphics_engine.hpp"
#include "objects/object.hpp"

#include <algorithm>
#include <iostream>


template<typename GraphicsEngineT>
std::optional<VkExtent2D> GraphicsEngineSwapChain<GraphicsEngineT>::swap_chain_extent;

template<typename GraphicsEngineT>
GraphicsEngineSwapChain<GraphicsEngineT>::GraphicsEngineSwapChain(GraphicsEngineT& engine) : GraphicsEngineBaseModule<GraphicsEngineT>(engine)
{
	create_render_pass();

	SwapChainSupportDetails swap_chain_support = query_swap_chain_support(get_physical_device(), get_graphics_engine().get_window_surface());
	VkSurfaceFormatKHR surface_format = choose_swap_surface_format(swap_chain_support.formats);
	VkPresentModeKHR present_mode = choose_swap_present_mode(swap_chain_support.presentModes);
	const auto extent = get_extent();

	{
		get_graphics_engine().create_image(extent.width, 
											extent.height,
											get_image_format(),
											VK_IMAGE_TILING_OPTIMAL,
											VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
											VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
											colorImage,
											colorImageMemory,
											msaa_samples);
		colorImageView = get_graphics_engine().create_image_view(colorImage, 
																get_image_format(),
																VK_IMAGE_ASPECT_COLOR_BIT);
	}

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
	create_info.imageExtent = extent;
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
		throw std::runtime_error("GraphicsEngineSwapChain::GraphicsEngineSwapChain() ERROR in num swapchain images!");
	}
	for (auto &handle : swap_chain_images)
	{
		// create the frames
		frames.emplace_back(get_graphics_engine(), *this, handle);
	}

	std::cout << "swap chain created, count=" << image_count << std::endl;
}

template<typename GraphicsEngineT>
GraphicsEngineSwapChain<GraphicsEngineT>::~GraphicsEngineSwapChain()
{
	vkDestroyRenderPass(get_logical_device(), render_pass, nullptr);

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

	vkDestroyImageView(get_logical_device(), colorImageView, nullptr);
	vkDestroyImage(get_logical_device(), colorImage, nullptr);
	vkFreeMemory(get_logical_device(), colorImageMemory, nullptr);
}

template<typename GraphicsEngineT>
void GraphicsEngineSwapChain<GraphicsEngineT>::reset()
{
	auto &engine = get_graphics_engine();
	this->~GraphicsEngineSwapChain();
	new (this) GraphicsEngineSwapChain(engine);
}

template<typename GraphicsEngineT>
VkSurfaceFormatKHR GraphicsEngineSwapChain<GraphicsEngineT>::choose_swap_surface_format(const std::vector<VkSurfaceFormatKHR> &available_formats)
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

template<typename GraphicsEngineT>
VkPresentModeKHR GraphicsEngineSwapChain<GraphicsEngineT>::choose_swap_present_mode(const std::vector<VkPresentModeKHR> &available_present_modes)
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

// template<typename GraphicsEngineT>
// void GraphicsEngineSwapChain<GraphicsEngineT>::recreate_swap_chain()
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

template<typename GraphicsEngineT>
void GraphicsEngineSwapChain<GraphicsEngineT>::spawn_object(GraphicsEngineObject<GraphicsEngineT>& object)
{
	for (auto& frame : frames)
	{
		frame.spawn_object(object);
	}
}

template<typename GraphicsEngineT>
void GraphicsEngineSwapChain<GraphicsEngineT>::draw()
{
	get_curr_frame().draw();

	current_frame = (current_frame + 1) % frames.size();
}

template<typename GraphicsEngineT>
void GraphicsEngineSwapChain<GraphicsEngineT>::update_command_buffer()
{
	for (auto& frame : frames)
	{
		frame.update_command_buffer();
	}
}

template<typename GraphicsEngineT>
void GraphicsEngineSwapChain<GraphicsEngineT>::create_render_pass()
{
	//
	// Color Attachment
	//
	VkAttachmentDescription color_attachment{};
	color_attachment.format = get_image_format();
	color_attachment.samples = get_msaa_samples(); // >1 if we are doing multisampling	
	color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; // determine what to do with the data in the attachment before rendering
	color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE; // dtermine what to do with the data in the attachment after rendering
	color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; // specifies which layout the image will have before the render pass begins
	// color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR; // specifies the layout to automatically transition to when the render pass finishes
	// with multisampled images, we don't want to present them directly, first they have to be resolved
	// to a single regular image
	color_attachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	// subpasses and attachment references
	// a single render pass can consist of multiple subpasses
	VkAttachmentReference color_attachment_ref{};
	color_attachment_ref.attachment = 0; // only works since we only have 1 attachment description
	color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	//
	// Depth Attachment
	//
	VkAttachmentDescription depth_attachment{};
	depth_attachment.format = GraphicsEngineDepthBuffer<GraphicsEngineT>::findDepthFormat(get_physical_device());
	depth_attachment.samples = get_msaa_samples();
	depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depth_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depth_attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depthAttachmentRef{};
	depthAttachmentRef.attachment = 1;
	depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	//
	// Multi-sampling
	//
	VkAttachmentDescription color_attachment_resolve{};
    color_attachment_resolve.format = get_image_format();
    color_attachment_resolve.samples = VK_SAMPLE_COUNT_1_BIT;
    color_attachment_resolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment_resolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment_resolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment_resolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment_resolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_attachment_resolve.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference color_attachment_resolve_ref{};
	color_attachment_resolve_ref.attachment = 2;
	color_attachment_resolve_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	//
	// Subpass
	//
	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS; // as opposed to compute subpass
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &color_attachment_ref;
	// subpass.pInputAttachments // attachments that read from a shader
	subpass.pResolveAttachments = &color_attachment_resolve_ref;
	subpass.pDepthStencilAttachment = &depthAttachmentRef;
	// subpass.pPreserveAttachments // attachments that are not used by this subpass, but for which the data must be preserved

	// render pass
	VkSubpassDependency dependency{};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	// depth image is accessed early in the frament test pipeline stage
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dependency.srcAccessMask = 0;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

	std::vector<VkAttachmentDescription> attachments{ color_attachment, depth_attachment, color_attachment_resolve };
	VkRenderPassCreateInfo render_pass_create_info{};
	render_pass_create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	render_pass_create_info.attachmentCount = attachments.size();
	render_pass_create_info.pAttachments = attachments.data();
	render_pass_create_info.subpassCount = 1;
	render_pass_create_info.pSubpasses = &subpass;
	render_pass_create_info.dependencyCount = 1;
	render_pass_create_info.pDependencies = &dependency;
	
	if (vkCreateRenderPass(get_logical_device(), &render_pass_create_info, nullptr, &render_pass) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create render pass!");
	}
}

template<typename GraphicsEngineT>
VkExtent2D GraphicsEngineSwapChain<GraphicsEngineT>::get_extent()
{
	return get_extent(get_physical_device(), get_graphics_engine().get_window_surface());
}

template<typename GraphicsEngineT>
VkExtent2D GraphicsEngineSwapChain<GraphicsEngineT>::get_extent(VkPhysicalDevice physical_device, VkSurfaceKHR window_surface)
{
	if (!swap_chain_extent.has_value())
	{
		SwapChainSupportDetails swap_chain_support = query_swap_chain_support(physical_device, window_surface);
		swap_chain_extent = choose_swap_extent(window_surface, swap_chain_support.capabilities);
	}

	return *swap_chain_extent;
}

template<typename GraphicsEngineT>
SwapChainSupportDetails GraphicsEngineSwapChain<GraphicsEngineT>::query_swap_chain_support(VkPhysicalDevice physical_device, VkSurfaceKHR window_surface)
{
	SwapChainSupportDetails details;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, window_surface, &details.capabilities);

	uint32_t format_count;
	vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, window_surface, &format_count, nullptr);

	if (format_count)
	{
		details.formats.resize(format_count);
		vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, window_surface, &format_count, details.formats.data());
	}

	uint32_t present_mode_count;
	vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, window_surface, &present_mode_count, nullptr);

	if (present_mode_count)
	{
		details.presentModes.resize(present_mode_count);
		vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, window_surface, &present_mode_count, details.presentModes.data());
	}

	return details;
}

// extent = resolution of the swap chain images and ~ resolution of window we are drawing to
template<typename GraphicsEngineT>
VkExtent2D GraphicsEngineSwapChain<GraphicsEngineT>::choose_swap_extent(VkSurfaceKHR window_surface, const VkSurfaceCapabilitiesKHR& capabilities)
{
	// the reason the logic to get the extent is a bit convoluted is because sometimes there might
	// be a difference between "extent" and "resolution". With most displays extent == resolution,
	// however with Apple's Retina display there might be more pixels per "point"
	if (capabilities.currentExtent.width != UINT32_MAX)
	{
		return capabilities.currentExtent;
	}
	else
	{
		// QUICK HACK to get this to wrok, would need better fix in future
		throw std::runtime_error("GraphicsEngineSwapChain::choose_swap_extent: invalid capabiliies!");
		return VkExtent2D{};

		// int width, height;
		// glfwGetFramebufferSize(window_surface, &width, &height);
		// VkExtent2D actual_extent = {
		// 	static_cast<uint32_t>(width),
		// 	static_cast<uint32_t>(height)};

		// actual_extent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, actual_extent.width));
		// actual_extent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, actual_extent.height));

		// return actual_extent;
	}
}