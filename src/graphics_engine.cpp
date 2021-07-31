#include "graphics_engine.hpp"

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

static void frame_buffer_resize_callback(GLFWwindow* window, int width, int height)
{
	auto app = reinterpret_cast<GraphicsEngine*>(glfwGetWindowUserPointer(window));
	app->set_frame_buffer_resized();
}

//
// static functions
//

void GraphicsEngine::initWindow() {
	glfwInit();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	// glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
	window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
	glfwSetWindowUserPointer(window, this);
	glfwSetFramebufferSizeCallback(window, frame_buffer_resize_callback);
}

void GraphicsEngine::initVulkan() {
	createInstance();
	create_window_surface();
	setupDebugMessenger();
	pick_physical_device();
	create_logical_device();
	create_swap_chain();
	create_image_views();
	create_render_pass();
	create_graphics_pipeline();
	create_frame_buffers();
	create_command_pool();
	create_vertex_buffer(); // remove this
	create_command_buffers();
	create_synchronisation_objects();
	create_vertex_buffer();
}

void GraphicsEngine::createInstance() 
{
	VkApplicationInfo app_info{};
	app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	app_info.pApplicationName = "Hello Triangle";
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

	// vulkan is platform agnostic and therefore an extension is necessary 
	std::vector<std::string> required_extensions = get_required_extensions();
	std::cout << "required extensions:\n";
	for (auto& required_extension : required_extensions)
	{
		std::cout << '\t' << required_extension << '\n';
	}
	auto required_extensions_old = c_style_str_array(required_extensions);
	create_info.enabledExtensionCount = required_extensions_old.size();
	create_info.ppEnabledExtensionNames = required_extensions_old.data();
	
	uint32_t extensionCount = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
	std::vector<VkExtensionProperties> extensions(extensionCount);
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());
	std::cout << "available extensions:\n";
	for (const auto& extension : extensions) {
		std::cout << '\t' << extension.extensionName << '\n';
	}

	if (vkCreateInstance(&create_info, nullptr, &instance) != VK_SUCCESS) {
		throw std::runtime_error("failed to create instance!");
	}
}

void GraphicsEngine::create_window_surface()
{
	if (glfwCreateWindowSurface(instance, window, nullptr, &window_surface) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create window surface!");
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
		if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
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
	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || frame_buffer_resized) {
		frame_buffer_resized = false;
		recreate_swap_chain();
	} else if (result != VK_SUCCESS) {
		throw std::runtime_error("failed to present swap chain image!");
	}
}

void GraphicsEngine::create_vertex_buffer()
{
	if (vertices.size() == 0)
	{
		throw std::runtime_error("vertices cannot be 0");
	}
	VkBufferCreateInfo buffer_create_info{};
	buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	buffer_create_info.size = sizeof(vertices[0]) * vertices.size();
	buffer_create_info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	buffer_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE; // will only be used in graphics queue

	if (vkCreateBuffer(logical_device, &buffer_create_info, nullptr, &vertex_buffer) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create vertex buffer!");
	}

	vkGetBufferMemoryRequirements(logical_device, vertex_buffer, &memory_requirements);

	// graphics cards offer different types of memory to allocate from, each type of memory varies
	// in therms of allowed operations and performance characteristics
	auto find_memory_type = [this](uint32_t type_filter, VkMemoryPropertyFlags property_flags)
	{
		VkPhysicalDeviceMemoryProperties memory_properties;
		vkGetPhysicalDeviceMemoryProperties(this->physicalDevice, &memory_properties);

		for (uint32_t i = 0; i < memory_properties.memoryTypeCount; i++)
		{
			if ((type_filter & (1 << i)) && ((memory_properties.memoryTypes[i].propertyFlags & property_flags) == property_flags))
			{
				return i;
			}
		}

		throw std::runtime_error("failed to find suitable memory type!");
	};

	VkMemoryAllocateInfo memory_allocate_info{};
	memory_allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memory_allocate_info.allocationSize = memory_requirements.size;
	memory_allocate_info.memoryTypeIndex = find_memory_type(
		memory_requirements.memoryTypeBits, 
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
		VK_MEMORY_PROPERTY_HOST_COHERENT_BIT); // make sure that the memory heap is host coherent
		// this is because the driver may not immediately copy the data into the buffer memory

	// allocate memory
	if (vkAllocateMemory(logical_device, &memory_allocate_info, nullptr, &vertex_buffer_memory) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to allocate vertex buffer memory!");
	}

	// bind memory
	vkBindBufferMemory(logical_device, vertex_buffer, vertex_buffer_memory, 0);

	//
	// filling the vertex buffer
	//

	// copy the vertex data to the buffer, this is done by mapping the buffer memory into CPU accessible memory with vkMapMemory
	void* data;
	// access a region of the specified memory resource
	vkMapMemory(logical_device, vertex_buffer_memory, 0, buffer_create_info.size, 0, &data);
	memcpy(data, vertices.data(), static_cast<size_t>(buffer_create_info.size));
	vkUnmapMemory(logical_device, vertex_buffer_memory);
}

void GraphicsEngine::cleanup() 
{
	std::cout<<"cleaning up\n";
	vkDeviceWaitIdle(logical_device);

	clean_up_swap_chain();

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
	glfwDestroyWindow(window);
	glfwTerminate();
}