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
	game_engine(_game_engine),
	instance(*this),
	validation_layer(*this),
	texture_mgr(*this),
	device(*this),
	pool(*this),
	swap_chain(*this)
{
}

GraphicsEngine::~GraphicsEngine() 
{
	std::cout<<"cleaning up\n";
	vkDeviceWaitIdle(get_logical_device());

	// TODO: might need to adjust order here
	vkDestroyPipeline(get_logical_device(), graphics_engine_pipeline, nullptr);
	vkDestroyPipelineLayout(get_logical_device(), pipeline_layout, nullptr);

	texture_mgr.cleanup();

	vkDestroyDescriptorSetLayout(get_logical_device(), descriptor_set_layout, nullptr);

	// moved to graphics engine object
	// vkDestroyBuffer(get_logical_device(), vertex_buffer, nullptr);
	// vkFreeMemory(get_logical_device(), vertex_buffer_memory, nullptr);

	//vkDestroyCommandPool(get_logical_device(), command_pool, nullptr);

	vkDestroySurfaceKHR(get_instance(), window_surface, nullptr);
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
	create_descriptor_set_layout();
	create_graphics_pipeline();
	// create_frame_buffers();
	// create_command_pool();
	texture_mgr.init();
	// create_vertex_buffer(); // moved to graphics engine object
	// create_uniform_buffers();
	create_descriptor_pools();
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
		vkGetPhysicalDeviceSurfaceSupportKHR(device, i, window_surface, &present_support);
		if (present_support)
		{
			indices.presentFamily = i;
		}
	}

	return indices;
}

// void GraphicsEngine::draw_frame()
// {
// 	// 1. acquire image from swap chain
// 	// 2. execute command buffer with image as attachment in the frame buffer
// 	// 3. return the image to swap chain for presentation

// 	// CPU-GPU synchronisation for in flight images
// 	vkWaitForFences(get_logical_device(), 1, &in_flight_fences[current_frame], VK_TRUE, std::numeric_limits<uint64_t>::max());

// 	unsigned image_index;
// 	// using uint64 max disables timeout
// 	VkResult result = vkAcquireNextImageKHR(get_logical_device(), swap_chain, std::numeric_limits<uint64_t>::max(), 
// 		image_available_semaphores[current_frame], VK_NULL_HANDLE, &image_index);
// 	if (result == VK_ERROR_OUT_OF_DATE_KHR) {
// 		recreate_swap_chain();
// 		return;
// 	} else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
// 		throw std::runtime_error("failed to acquire swap chain image!");
// 	}

// 	// check if a previous frame is using this image (i.e. there is it fence to wait on)
// 	if (images_in_flight[image_index] != VK_NULL_HANDLE)
// 	{
// 		vkWaitForFences(get_logical_device(), 1, &images_in_flight[image_index], VK_TRUE, std::numeric_limits<uint64_t>::max());
// 	}
// 	// mark the image as now being in use by this frame
// 	images_in_flight[image_index] = in_flight_fences[current_frame];		

// 	update_uniform_buffer(image_index);

// 	//
// 	// submitting the command buffer
// 	//

// 	VkSubmitInfo submitInfo{};
// 	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

// 	VkSemaphore waitSemaphores[] = { image_available_semaphores[current_frame] };
// 	VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
// 	// here we specify which semaphore to wait on before execution begins and in which stage of the pipeline to wait
// 	submitInfo.waitSemaphoreCount = 1;
// 	submitInfo.pWaitSemaphores = waitSemaphores;
// 	submitInfo.pWaitDstStageMask = waitStages;

// 	// here we specify which command buffers to actually submit for execution
// 	submitInfo.commandBufferCount = 1;
// 	submitInfo.pCommandBuffers = &command_buffers[image_index];

// 	// here we specify the semaphores to signal once the command buffer has finished execution
// 	VkSemaphore signal_semaphores[] = { render_finished_semaphores[current_frame] };
// 	submitInfo.signalSemaphoreCount = 1;
// 	submitInfo.pSignalSemaphores = signal_semaphores;

// 	vkResetFences(get_logical_device(), 1, &in_flight_fences[current_frame]);
// 	if (vkQueueSubmit(graphics_queue, 1, &submitInfo, in_flight_fences[current_frame]) != VK_SUCCESS)
// 	{
// 		throw std::runtime_error("failed to submit draw command buffer!");
// 	}

// 	//
// 	// presentation
// 	//

// 	VkPresentInfoKHR present_info{};
// 	present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
// 	present_info.waitSemaphoreCount = 1;
// 	present_info.pWaitSemaphores = signal_semaphores;

// 	VkSwapchainKHR swap_chains[] = { swap_chain };
// 	present_info.swapchainCount = 1;
// 	present_info.pSwapchains = swap_chains;
// 	present_info.pImageIndices = &image_index;
// 	present_info.pResults = nullptr; // allows you to specify array of VkResult values to check for every individual swap chain if presentation was successful

// 	current_frame = (current_frame + 1) % MAX_FRAMES_IN_FLIGHT;

// 	result = vkQueuePresentKHR(present_queue, &present_info);
// 	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || frame_buffer_resized) { // recreate if we resize window
// 		frame_buffer_resized = false;
// 		recreate_swap_chain();
// 	} else if (result != VK_SUCCESS) {
// 		throw std::runtime_error("failed to present swap chain image!");
// 	}
// }

// void GraphicsEngine::update_uniform_buffer(uint32_t image_index)
// {
// 	static auto start_time = std::chrono::high_resolution_clock::now(); // static here means that start_time is initialised only once
// 	auto current_time = std::chrono::high_resolution_clock::now();
// 	float time = std::chrono::duration<float, std::chrono::seconds::period>(current_time - start_time).count();

// 	UniformBufferObject default_ubo{};
// 	default_ubo.model = glm::mat4(1);
// 	default_ubo.view = get_camera()->get_view(); // we can move this to push constant
// 	default_ubo.proj = get_camera()->get_perspective(); // we can move this to push constant
// 	// default_ubo.proj[1][1] *= -1; // ubo was originally designed for opengl whereby its y axis is flipped

// 	std::vector<UniformBufferObject> ubos(get_objects().size(), default_ubo);
// 	for (int i = 0; i < ubos.size(); i++)
// 	{
// 		ubos[i].model = objects[i]->get_transformation();
// 	}

// 	// should we keep this data mapped?
// 	void* data;
// 	size_t size = ubos.size() * sizeof(ubos[0]);
// 	vkMapMemory(get_logical_device(), uniform_buffers_memory[image_index], 0, size, 0, &data);
// 	memcpy(data, ubos.data(), size);
// 	vkUnmapMemory(get_logical_device(), uniform_buffers_memory[image_index]);
// }

void GraphicsEngine::run() {
	while (!should_shutdown)
	{
		std::lock_guard<std::mutex> lock(ge_cmd_q_mutex);
		if (!ge_cmd_q.empty())
		{
			ge_cmd_q.front()->process(this);
			ge_cmd_q.pop();
		}

		swap_chain.draw();

		std::this_thread::sleep_for(std::chrono::milliseconds(10));
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