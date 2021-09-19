#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <vector>
#include <optional>
#include <set>

#include "vertex.hpp"
#include "graphics_engine_validation_layer.hpp"
#include "queues.hpp"
#include "utility_functions.hpp"
#include "graphics_engine_texture.hpp"


// forward declares
class Camera;
class GameEngine;
class Object;

struct SwapChainSupportDetails
{
	VkSurfaceCapabilitiesKHR capabilities;
	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR> presentModes;
};

// be vary of alignment issues
struct UniformBufferObject
{
	glm::mat4 model{};
	glm::mat4 view;
	glm::mat4 proj;
};

class GraphicsEngine {
public:
	GraphicsEngine() = delete;
	GraphicsEngine(GameEngine& _game_engine);
	~GraphicsEngine();

	void setup() { initVulkan(); }
	void run() { mainLoop(); }
	void set_frame_buffer_resized() { frame_buffer_resized = true; }
	void shutdown() { should_shutdown = true; }
	const std::vector<VkImage>& get_swap_chain() { return swap_chain_images; }

public: // getters and setters
	template<class T> 
	T get_window_width() const { return (T)swap_chain_extent.width; }
	template<class T>
	T get_window_height() const { return (T)swap_chain_extent.height; }
	GLFWwindow* get_window();
	Camera* get_camera();
	void add_vertex_set(const std::vector<Vertex>& vertex_set) { vertex_sets.emplace_back(vertex_set); }
	std::vector<std::vector<Vertex>>& get_vertex_sets() { return vertex_sets; }
	void insert_object(Object* object);

private:
	// the instance is the connection between application and Vulkan library
	// its creation involves specifying some details about your application to the driver
	VkInstance instance; 
	VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
	VkDevice logical_device;
	VkQueue graphics_queue;
	VkQueue present_queue;
	VkSurfaceKHR window_surface;
	VkSwapchainKHR swap_chain;
	std::vector<VkImage> swap_chain_images; // handle of the swap chain images
	VkFormat swap_chain_image_format;
	VkExtent2D swap_chain_extent;
	VkDescriptorSetLayout descriptor_set_layout;
	VkPipelineLayout pipeline_layout;
	std::vector<VkImageView> swap_chain_image_views;
	VkRenderPass render_pass;
	VkPipeline graphics_engine_pipeline;
	std::vector<VkFramebuffer> swap_chain_frame_buffers;
	VkCommandPool command_pool;
	std::vector<VkCommandBuffer> command_buffers;
	std::vector<VkSemaphore> image_available_semaphores;
	std::vector<VkSemaphore> render_finished_semaphores;
	std::vector<VkFence> in_flight_fences;
	std::vector<VkFence> images_in_flight;
	VkDebugUtilsMessengerEXT debug_messenger;
	std::vector<std::vector<Vertex>> vertex_sets;
	// swap chain
	const std::vector<std::string> device_extensions{ VK_KHR_SWAPCHAIN_EXTENSION_NAME };
	int current_frame = 0;
	bool frame_buffer_resized = false;
	const int MAX_FRAMES_IN_FLIGHT = 2;
	bool should_shutdown = false;
	bool is_initialised = false;
	GameEngine& game_engine;

public: // vertix buffers
	VkVertexInputBindingDescription binding_description;
	std::vector<VkVertexInputAttributeDescription> attribute_descriptions;
	VkBuffer vertex_buffer;
	VkDeviceMemory vertex_buffer_memory;

	// as opposed to vertex_buffers we expect to change uniform buffer every frame
	std::vector<VkBuffer> uniform_buffers;
	std::vector<VkDeviceMemory> uniform_buffers_memory;
	VkDescriptorPool descriptor_pool;
	std::vector<VkDescriptorSet> descriptor_sets;
	GraphicsEngineTexture texture_mgr;

private:
	void initVulkan();

	void createInstance();

	void pick_physical_device();

	void create_logical_device();

	QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);

	// semaphores and fences, for GPU-GPU and CPU-GPU synchronisation
	void create_synchronisation_objects();

// if confused about the different vulkan definitions see here
// https://stackoverflow.com/questions/39557141/what-is-the-difference-between-framebuffer-and-image-in-vulkan

public: // swap chain
	void create_swap_chain();
	SwapChainSupportDetails query_swap_chain_support(VkPhysicalDevice device);
	VkSurfaceFormatKHR choose_swap_surface_format(const std::vector<VkSurfaceFormatKHR>& available_formats);
	VkPresentModeKHR choose_swap_present_mode(const std::vector<VkPresentModeKHR>& available_present_modes);
	// extent = resolution of the swap chain images and ~ resolution of window we are drawing to
	VkExtent2D choose_swap_extent(const VkSurfaceCapabilitiesKHR& capabilities);

	void recreate_swap_chain(); // useful for when size of window is changing
	void clean_up_swap_chain();

public: // image views
	void create_image_views();

public: // graphics pipeline
	void create_graphics_pipeline();
	void create_render_pass();

public: // frame buffer
	void create_frame_buffers();

public: // command buffer
	void create_command_pool();
	void create_command_buffers();
	VkCommandBuffer begin_single_time_commands();
	void end_single_time_commands(VkCommandBuffer command_buffer);

public: // vertex buffer
	void create_vertex_buffer();

	void create_buffer(size_t size, 
					   VkBufferUsageFlags usage_flags, 
					   VkMemoryPropertyFlags memory_flags, 
					   VkBuffer& buffer, 
					   VkDeviceMemory& device_memory);

	void copy_buffer(VkBuffer src_buffer, VkBuffer dest_buffer, size_t size);

	void update_uniform_buffer(uint32_t current_image);

	void create_descriptor_pools();

	void create_descriptor_sets();

	bool bPhysicalDevicePropertiesCached = false;
	VkPhysicalDeviceProperties physical_device_properties;
	const VkPhysicalDeviceProperties& get_physical_device_properties()
	{
		// may want to add an assert here to make sure physical device is initialised
		if (!bPhysicalDevicePropertiesCached)
		{
			vkGetPhysicalDeviceProperties(physicalDevice, &physical_device_properties);			
			std::cout << "Cached physical device properties\n";
		}

		return physical_device_properties;
	}

private: //uniform buffer
	void create_descriptor_set_layout();
	void create_uniform_buffers();

public: // validation layer
	void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
	void setupDebugMessenger();
	VkResult CreateDebugUtilsMessengerEXT(
		VkInstance instance, 
		const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, 
		const VkAllocationCallbacks* pAllocator, 
		VkDebugUtilsMessengerEXT* pDebugMessenger);
	void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator);

public: // getters
	VkDevice& get_logical_device() { return logical_device; }

public: // extensions
	bool check_device_extension_support(VkPhysicalDevice device, std::vector<std::string> device_extensions);
	// graphics cards offer different types of memory to allocate from, each type of memory varies
	// in therms of allowed operations and performance characteristics
	int find_memory_type(uint32_t type_filter, VkMemoryPropertyFlags flags);

public: // main
	void draw_frame();

    void mainLoop();
};

