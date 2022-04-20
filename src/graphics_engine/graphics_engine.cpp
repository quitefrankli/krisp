#include "graphics_engine.hpp"

#include "camera.hpp"
#include "game_engine.hpp"
#include "objects/object.hpp"
#include "uniform_buffer_object.hpp"
#include "utility_functions.hpp"
#include "analytics.hpp"

#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/string_cast.hpp>
#include <ImGui/imgui_impl_vulkan.h>
#include <ImGui/imgui_impl_glfw.h>
#include <imgui.h>
#include <fmt/core.h>
#include <fmt/color.h>

#include <stdexcept>
#include <thread>
#include <chrono>
#include <vector>
#include <iostream>


GraphicsEngine::GraphicsEngine(GameEngine& _game_engine) : 
	binding_description(Vertex::get_binding_description()),
	attribute_descriptions(Vertex::get_attribute_descriptions()),
	game_engine(_game_engine),
	instance(*this),
	validation_layer(*this),
	texture_mgr(*this),
	device(*this),
	pool(*this),
	pipeline_mgr(*this),
	depth_buffer(*this),
	swap_chain(*this),
	gui_manager(*this)
{
	FPS_tracker = std::make_unique<Analytics>(
		[this](float fps) {
			get_gui_manager().update_fps(1e6 / fps);
		}, 1);
	FPS_tracker->text = "FPS Tracker";
}

GraphicsEngine::~GraphicsEngine() 
{
	std::cout<<"cleaning up\n";
	vkDeviceWaitIdle(get_logical_device());
}

Camera* GraphicsEngine::get_camera()
{
	return &(game_engine.get_camera());
}

GLFWwindow* GraphicsEngine::get_window()
{
	return game_engine.get_window();
}

QueueFamilyIndices GraphicsEngine::findQueueFamilies(VkPhysicalDevice device) {
	QueueFamilyIndices indices;
	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

	std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

	for (int i = 0; i < queueFamilies.size(); i++)
	{
		if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) // GRAPHICS_BIT also implicitly supports VK_QUEUE_TRANSFER_BIT
		{
			indices.graphicsFamily = i;
		}
		VkBool32 present_support = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(device, i, get_window_surface(), &present_support);
		if (present_support)
		{
			indices.presentFamily = i;
		}
	}

	return indices;
}

void GraphicsEngine::run() {
	try {
		Analytics analytics;
		analytics.text = "GraphicsEngine: average cycle ms";
		FPS_tracker->start();
		while (!should_shutdown)
		{
			// for FPS
			FPS_tracker->stop();
			FPS_tracker->start();

			analytics.start();
			ge_cmd_q_mutex.lock();
			while (!ge_cmd_q.empty())
			{
				ge_cmd_q.front()->process(this);
				ge_cmd_q.pop();
			}
			ge_cmd_q_mutex.unlock();

			gui_manager.draw();

			// swap chain draw should be last in execution loop
			swap_chain.draw();
			std::this_thread::sleep_for(std::chrono::milliseconds(10));

			analytics.stop();
		}
    } catch (const std::exception& e) {
		fmt::print(fg(fmt::color::red), "GraphicsEngine Exception Thrown!: {}\n", e.what());
        throw e;
	} catch (...) {
		fmt::print(fg(fmt::color::red), "GraphicsEngine Exception Thrown!: UNKNOWN\n");
        throw std::runtime_error("");
	}
}

int GraphicsEngine::find_memory_type(uint32_t type_filter, VkMemoryPropertyFlags flags)
{
	VkPhysicalDeviceMemoryProperties memory_properties;
	vkGetPhysicalDeviceMemoryProperties(get_physical_device(), &memory_properties);

	for (uint32_t i = 0; i < memory_properties.memoryTypeCount; i++)
	{
		if ((type_filter & (1 << i)) && ((memory_properties.memoryTypes[i].propertyFlags & flags) == flags))
		{
			return i;
		}
	}

	throw std::runtime_error("failed to find suitable memory type!");
};

void GraphicsEngine::recreate_swap_chain()
{
	// for when window is minimised
	int width = 0, height = 0;
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(get_window(), &width, &height);
        glfwWaitEvents();
    }

	vkDeviceWaitIdle(get_logical_device()); // we want to wait until resource is no longer in use

	swap_chain.reset();
}

void GraphicsEngine::enqueue_cmd(std::unique_ptr<GraphicsEngineCommand>&& cmd)
{
	std::lock_guard<std::mutex> lock(ge_cmd_q_mutex);
	ge_cmd_q.push(std::move(cmd));
}

VkCommandBuffer GraphicsEngine::begin_single_time_commands()
{
	VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = get_command_pool();
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(get_logical_device(), &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

 	// start recording the command buffer
    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    return commandBuffer;
}

void GraphicsEngine::end_single_time_commands(VkCommandBuffer command_buffer)
{
    vkEndCommandBuffer(command_buffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &command_buffer;

	// note that unlike draw stage, we don't need to wait for anything here except for the queue to become idle
    vkQueueSubmit(graphics_queue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphics_queue);

    vkFreeCommandBuffers(get_logical_device(), get_command_pool(), 1, &command_buffer);
}

VkExtent2D GraphicsEngine::get_extent_unsafe()
{
	int width, height;
	glfwGetWindowSize(get_window(), &width, &height);

	VkExtent2D extent;
	extent.width = width;
	extent.height = height;

	return extent;
}

void GraphicsEngine::update_command_buffer()
{
	swap_chain.update_command_buffer();
}

void GraphicsEngine::create_image(uint32_t width,
								uint32_t height,
								VkFormat format,
								VkImageTiling tiling,
								VkImageUsageFlags usage,
								VkMemoryPropertyFlags properties,
								VkImage &image,
								VkDeviceMemory &image_memory,
								VkSampleCountFlagBits sample_count_flag)
{
	VkImageCreateInfo image_info{};
	image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	image_info.imageType = VK_IMAGE_TYPE_2D; // 1D for array of data or gradient, 3D for voxels
	image_info.extent.width = static_cast<uint32_t>(width);
	image_info.extent.height = static_cast<uint32_t>(height);
	image_info.extent.depth = 1;
	image_info.mipLevels = 1; // mip mapping
	image_info.arrayLayers = 1;
	image_info.format = format;
	image_info.tiling = tiling;							  // types include:
														  // LINEAR - texels are laid out in row major order
														  // OPTIMAL - texels are laid out in an implementation defined order
	image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; // UNDEFINED = not usable by GPU and first transition will discard texels
														  // PREINITIALIZED = not usable by GPU and first transition will preserve texels
	image_info.usage = usage;
	image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE; // will only be used by one queue family
	image_info.samples = sample_count_flag;			// for multisampling
	image_info.flags = 0;

	if (vkCreateImage(get_logical_device(), &image_info, nullptr, &image) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create image!");
	}

	//
	// allocate memory for an image
	//

	VkMemoryRequirements mem_req;
	vkGetImageMemoryRequirements(get_logical_device(), image, &mem_req);
	VkMemoryAllocateInfo alloc_info{};
	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.allocationSize = mem_req.size;
	alloc_info.memoryTypeIndex = find_memory_type(mem_req.memoryTypeBits, properties);

	if (vkAllocateMemory(get_logical_device(), &alloc_info, nullptr, &image_memory) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to allocate image memory!");
	}

	vkBindImageMemory(get_logical_device(), image, image_memory, 0);
}

VkImageView GraphicsEngine::create_image_view(VkImage& image,
															VkFormat format,
															VkImageAspectFlags aspect_flags,
															VkImageViewType view_type)
{
	VkImageViewCreateInfo create_info{};
	create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	create_info.image = image;
	create_info.viewType = view_type; // specifies how the image data should be interpreted
												  // i.e. treat images as 1D, 2D, 3D textures and cube maps
	create_info.format = format;
	create_info.subresourceRange.aspectMask = aspect_flags; // describes image purpose and which part should be accessed
	create_info.subresourceRange.baseMipLevel = 0;
	create_info.subresourceRange.levelCount = 1;
	create_info.subresourceRange.baseArrayLayer = 0;
	create_info.subresourceRange.layerCount = 1;

	VkImageView image_view;
	if (vkCreateImageView(get_logical_device(), &create_info, nullptr, &image_view) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create image views!");
	}

	return image_view;
}