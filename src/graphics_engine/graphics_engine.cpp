#include "graphics_engine.hpp"

#include "camera.hpp"
#include "game_engine.hpp"
#include "objects.hpp"
#include "uniform_buffer_object.hpp"
#include "utility_functions.hpp"

#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/string_cast.hpp>

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
	pipeline(*this),
	swap_chain(*this)
{
}

GraphicsEngine::~GraphicsEngine() 
{
	std::cout<<"cleaning up\n";
	vkDeviceWaitIdle(get_logical_device());

	texture_mgr.cleanup();

	// moved to graphics engine object
	// vkDestroyBuffer(get_logical_device(), vertex_buffer, nullptr);
	// vkFreeMemory(get_logical_device(), vertex_buffer_memory, nullptr);

	//vkDestroyCommandPool(get_logical_device(), command_pool, nullptr);
}

Camera* GraphicsEngine::get_camera()
{
	return &(game_engine.get_camera());
}

GLFWwindow* GraphicsEngine::get_window()
{
	return game_engine.get_window();
}

void GraphicsEngine::setup() {
	//create_descriptor_set_layout(); // moved to pool
	//create_graphics_pipeline(); // moved to GraphicsEnginePipline
	// create_frame_buffers();
	// create_command_pool(); // moved to pool
	texture_mgr.init();
	// create_vertex_buffer(); // moved to graphics engine object
	// create_uniform_buffers();
	// create_descriptor_pools(); // moved to pool
	// create_descriptor_sets(); // moved to swap_chain_frame
	// create_command_buffers(); // moved to swap_chain_frame
	// create_synchronisation_objects(); moveed to swap_chain
	is_initialised = true;
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
	while (!should_shutdown)
	{
		ge_cmd_q_mutex.lock();
		while (!ge_cmd_q.empty())
		{
			ge_cmd_q.front()->process(this);
			ge_cmd_q.pop();
		}
		ge_cmd_q_mutex.unlock();

		swap_chain.draw();
		std::this_thread::sleep_for(std::chrono::milliseconds(20));
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

const VkPhysicalDeviceProperties& GraphicsEngine::get_physical_device_properties()
{
	if (!bPhysicalDevicePropertiesCached)
	{
		vkGetPhysicalDeviceProperties(get_physical_device(), &physical_device_properties);			
		std::cout << "Cached physical device properties\n";
	}

	return physical_device_properties;
}

void GraphicsEngine::spawn_object(Object& object)
{
	objects.emplace_back(*this, object);

	auto& new_obj = objects.back();
	create_vertex_buffer(new_obj);

	// uniform buffer
	create_buffer(sizeof(UniformBufferObject),
				  VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
				  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				  new_obj.uniform_buffer,
				  new_obj.uniform_buffer_memory);
	swap_chain.spawn_object(new_obj);
}

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

void GraphicsEngine::change_texture(const std::string& str)
{
	//
	// TODO fix this maybe?
	//

	// // wait until vulkan commands finish processing
	// vkQueueWaitIdle(graphics_queue);

	// texture_mgr.change_texture(str);

	// // if updating an already bound descriptor set we must make sure the old sets don't get reused in the command buffers
	// vkFreeCommandBuffers(get_logical_device(), command_pool, static_cast<uint32_t>(command_buffers.size()), command_buffers.data());

	// update_descriptor_sets();

	// create_command_buffers();
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