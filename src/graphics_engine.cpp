#include "graphics_engine.hpp"

#include "camera.hpp"
#include "game_engine.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/string_cast.hpp>
#include <thread>
#include <chrono>

//
// static functions
//

static std::vector<std::string> get_required_extensions()
{
	uint32_t glfwExtensionCount;
	const char** glfwExtensions;
	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	auto required_extensions = char_ptr_arr_to_str_vec(glfwExtensions, glfwExtensionCount);
	if (enableValidationLayers)
	{
		required_extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	}
	return required_extensions;
}

//
// static functions
//

GraphicsEngine::GraphicsEngine(GameEngine& _game_engine) : 
	game_engine(_game_engine),
	texture_mgr(*this)
{
	createInstance();
	if (glfwCreateWindowSurface(instance, get_window(), nullptr, &window_surface) != VK_SUCCESS) // create window surface
	{
		throw std::runtime_error("failed to create window surface!");
	}

	setupDebugMessenger();
	pick_physical_device();
	create_logical_device();
	create_swap_chain();
}

GraphicsEngine::~GraphicsEngine() 
{
	std::cout<<"cleaning up\n";
	vkDeviceWaitIdle(logical_device);

	clean_up_swap_chain();

	texture_mgr.cleanup();

	vkDestroyDescriptorSetLayout(logical_device, descriptor_set_layout, nullptr);

	vkDestroyBuffer(logical_device, vertex_buffer, nullptr);
	vkFreeMemory(logical_device, vertex_buffer_memory, nullptr);

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		vkDestroySemaphore(logical_device, image_available_semaphores[i], nullptr);
		vkDestroySemaphore(logical_device, render_finished_semaphores[i], nullptr);
		vkDestroyFence(logical_device, in_flight_fences[i], nullptr);
	}
	vkDestroyCommandPool(logical_device, command_pool, nullptr);

	vkDestroyDevice(logical_device, nullptr);
	if (enableValidationLayers)
	{
		DestroyDebugUtilsMessengerEXT(instance, debug_messenger, nullptr);
	}
	vkDestroySurfaceKHR(instance, window_surface, nullptr);
	vkDestroyInstance(instance, nullptr);
}

Camera* GraphicsEngine::get_camera()
{
	return &(game_engine.get_camera());
}

GLFWwindow* GraphicsEngine::get_window()
{
	return game_engine.get_window();
}

void GraphicsEngine::initVulkan() {
	create_image_views();
	create_render_pass();
	create_descriptor_set_layout();
	create_graphics_pipeline();
	create_frame_buffers();
	create_command_pool();
	texture_mgr.init();
	create_vertex_buffer();
	create_uniform_buffers();
	create_descriptor_pools();
	create_descriptor_sets();
	create_command_buffers();
	create_synchronisation_objects();
	is_initialised = true;
}

void GraphicsEngine::createInstance() 
{
	VkApplicationInfo app_info{};
	app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	app_info.pApplicationName = "PNG VIEWER";
	app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	app_info.pEngineName = "No Engine";
	app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	app_info.apiVersion = VK_API_VERSION_1_0;

	VkInstanceCreateInfo create_info{};
	create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	create_info.pApplicationInfo = &app_info;
	if (enableValidationLayers) { // see validation_layer.hpp
		if (!checkValidationLayerSupport()) {
			throw std::runtime_error("validation layers requested, but not available!");
		}
		create_info.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
		create_info.ppEnabledLayerNames = validationLayers.data();
	} else {
		create_info.enabledLayerCount = 0;
	}

	// validation layer stuff
	VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
	populateDebugMessengerCreateInfo(debugCreateInfo);
	create_info.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;

	uint32_t extensionCount = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
	std::vector<VkExtensionProperties> extensions(extensionCount);
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());
	std::cout << "Available extensions:\n";
	for (const auto& extension : extensions) {
		std::cout << '\t' << extension.extensionName << '\n';
	}

	// vulkan is platform agnostic and therefore an extension is necessary 
	std::vector<std::string> required_extensions = get_required_extensions();
	std::cout << "Required extensions:\n";
	for (auto& required_extension : required_extensions)
	{
		std::cout << '\t' << required_extension << '\n';
	}
	auto required_extensions_old = c_style_str_array(required_extensions);
	create_info.enabledExtensionCount = required_extensions_old.size();
	create_info.ppEnabledExtensionNames = required_extensions_old.data();

	if (vkCreateInstance(&create_info, nullptr, &instance) != VK_SUCCESS) {
		throw std::runtime_error("failed to create instance!");
	}
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

void GraphicsEngine::draw_frame()
{
	// 1. acquire image from swap chain
	// 2. execute command buffer with image as attachment in the frame buffer
	// 3. return the image to swap chain for presentation

	// CPU-GPU synchronisation for in flight images
	vkWaitForFences(logical_device, 1, &in_flight_fences[current_frame], VK_TRUE, std::numeric_limits<uint64_t>::max());

	unsigned image_index;
	// using uint64 max disables timeout
	VkResult result = vkAcquireNextImageKHR(logical_device, swap_chain, std::numeric_limits<uint64_t>::max(), 
		image_available_semaphores[current_frame], VK_NULL_HANDLE, &image_index);
	if (result == VK_ERROR_OUT_OF_DATE_KHR) {
		recreate_swap_chain();
		return;
	} else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
		throw std::runtime_error("failed to acquire swap chain image!");
	}

	// check if a previous frame is using this image (i.e. there is it fence to wait on)
	if (images_in_flight[image_index] != VK_NULL_HANDLE)
	{
		vkWaitForFences(logical_device, 1, &images_in_flight[image_index], VK_TRUE, std::numeric_limits<uint64_t>::max());
	}
	// mark the image as now being in use by this frame
	images_in_flight[image_index] = in_flight_fences[current_frame];		

	update_uniform_buffer(image_index);

	//
	// submitting the command buffer
	//

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	VkSemaphore waitSemaphores[] = { image_available_semaphores[current_frame] };
	VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
	// here we specify which semaphore to wait on before execution begins and in which stage of the pipeline to wait
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = waitSemaphores;
	submitInfo.pWaitDstStageMask = waitStages;

	// here we specify which command buffers to actually submit for execution
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &command_buffers[image_index];

	// here we specify the semaphores to signal once the command buffer has finished execution
	VkSemaphore signal_semaphores[] = { render_finished_semaphores[current_frame] };
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = signal_semaphores;

	vkResetFences(logical_device, 1, &in_flight_fences[current_frame]);
	if (vkQueueSubmit(graphics_queue, 1, &submitInfo, in_flight_fences[current_frame]) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to submit draw command buffer!");
	}

	//
	// presentation
	//

	VkPresentInfoKHR present_info{};
	present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	present_info.waitSemaphoreCount = 1;
	present_info.pWaitSemaphores = signal_semaphores;

	VkSwapchainKHR swap_chains[] = { swap_chain };
	present_info.swapchainCount = 1;
	present_info.pSwapchains = swap_chains;
	present_info.pImageIndices = &image_index;
	present_info.pResults = nullptr; // allows you to specify array of VkResult values to check for every individual swap chain if presentation was successful

	current_frame = (current_frame + 1) % MAX_FRAMES_IN_FLIGHT;

	result = vkQueuePresentKHR(present_queue, &present_info);
	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || frame_buffer_resized) { // recreate if we resize window
		frame_buffer_resized = false;
		recreate_swap_chain();
	} else if (result != VK_SUCCESS) {
		throw std::runtime_error("failed to present swap chain image!");
	}
}

void GraphicsEngine::create_uniform_buffers()
{
	VkDeviceSize buffer_size = sizeof(UniformBufferObject) * nObjects;

	uniform_buffers.resize(swap_chain_images.size());
	uniform_buffers_memory.resize(swap_chain_images.size());

	for (size_t i = 0; i < swap_chain_images.size(); i++)
	{
		create_buffer(buffer_size,
					  VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
					  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
					  uniform_buffers[i],
					  uniform_buffers_memory[i]);
	}
}

void GraphicsEngine::update_uniform_buffer(uint32_t image_index)
{
	static auto start_time = std::chrono::high_resolution_clock::now(); // static here means that start_time is initialised only once
	auto current_time = std::chrono::high_resolution_clock::now();
	float time = std::chrono::duration<float, std::chrono::seconds::period>(current_time - start_time).count();

	UniformBufferObject default_ubo{};
	default_ubo.view = get_camera()->get_view();
	default_ubo.proj = get_camera()->get_perspective();	
	default_ubo.proj[1][1] *= -1; // ubo was originally designed for opengl whereby its y axis is flipped
	// std::cout << glm::to_string(get_camera()->get_perspective()) << '\n';
	// std::cout << glm::to_string(default_ubo.view) << '\n';
	std::vector<UniformBufferObject> ubos(nObjects, default_ubo);
	for (int i = 0; i < ubos.size(); i++)
	{
		ubos[i].model = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 1.0f, 0.0f) * float(i));
	}

	void* data;
	int size = ubos.size() * sizeof(ubos[0]);
	vkMapMemory(logical_device, uniform_buffers_memory[image_index], 0, size, 0, &data);
	memcpy(data, ubos.data(), size);
	vkUnmapMemory(logical_device, uniform_buffers_memory[image_index]);
}

void GraphicsEngine::mainLoop() {
	while (!should_shutdown)
	{
		draw_frame();

		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
}

int GraphicsEngine::find_memory_type(uint32_t type_filter, VkMemoryPropertyFlags flags)
{
	VkPhysicalDeviceMemoryProperties memory_properties;
	vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memory_properties);

	for (uint32_t i = 0; i < memory_properties.memoryTypeCount; i++)
	{
		if ((type_filter & (1 << i)) && ((memory_properties.memoryTypes[i].propertyFlags & flags) == flags))
		{
			return i;
		}
	}

	throw std::runtime_error("failed to find suitable memory type!");
};