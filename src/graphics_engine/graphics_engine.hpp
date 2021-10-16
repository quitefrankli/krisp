#pragma once

#include "graphics_engine_texture.hpp"
#include "graphics_engine_validation_layer.hpp"
#include "graphics_engine_swap_chain.hpp"
#include "graphics_engine_instance.hpp"
#include "graphics_engine_device.hpp"
#include "graphics_engine_pool.hpp"

#include "vertex.hpp"
#include "queues.hpp"

#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>

#include <vector>


class Camera;
class GameEngine;
class Object;


class GraphicsEngine {
public:
	GraphicsEngine() = delete;
	GraphicsEngine(GameEngine& _game_engine);
	~GraphicsEngine();

	void setup();
	void run();
	void shutdown() { should_shutdown = true; }

	const int MAX_NUM_OBJECTS = 10;
	const int MAX_NUM_DESCRIPTOR_SETS = MAX_NUM_OBJECTS * 100;
	const int MAX_NUM_VERTICES = MAX_NUM_DESCRIPTOR_SETS * 3;

public: // getters and setters
	template<class T> 
	T get_window_width() const { return static_cast<T>(swap_chain.get_extent().width); }
	template<class T>
	T get_window_height() const { return static_cast<T>(swap_chain.get_extent().height); }
	GLFWwindow* get_window();
	Camera* get_camera();
	void add_vertex_set(const std::vector<Vertex>& vertex_set) { vertex_sets.emplace_back(vertex_set); }
	std::vector<std::vector<Vertex>>& get_vertex_sets();
	void insert_object(Object* object);
	std::vector<Object*>& get_objects() { return objects; }
	inline VkDevice& get_logical_device() { return device.get_logical_device(); }
	inline VkPhysicalDevice& get_physical_device() { return device.get_physical_device(); }
	inline VkInstance& get_instance() { return instance.get(); }
	inline VkQueue& get_present_queue() { return present_queue; }
	inline VkQueue& get_graphics_queue() { return graphics_queue; }
	inline VkSurfaceKHR& get_window_surface() { return window_surface; }
	inline GraphicsEngineSwapChain& get_swap_chain() { return swap_chain; }
	VkRenderPass& get_render_pass() { return swap_chain.get_render_pass(); }
	uint32_t get_num_swapchain_images() { return swap_chain.get_num_images(); }
	VkDescriptorSetLayout& get_descriptor_set_layout() { return descriptor_set_layout; }
	VkDescriptorPool& get_descriptor_pool() { return descriptor_pool; }
	VkCommandPool& get_command_pool() { return pool.get_command_pool(); }
	VkPipeline& get_graphics_pipeline() { return graphics_engine_pipeline; }

private:
	GameEngine& game_engine;
	GraphicsEngineInstance instance;
	GraphicsEngineValidationLayer validation_layer;
	GraphicsEngineDevice device;
	GraphicsEnginePool pool;
	GraphicsEngineSwapChain swap_chain;
	VkQueue graphics_queue;
	VkQueue present_queue;
	VkSurfaceKHR window_surface;
	VkDescriptorSetLayout descriptor_set_layout;
	VkPipeline graphics_engine_pipeline;
	std::vector<std::vector<Vertex>> vertex_sets;
	std::vector<Object*> objects;
	VkDescriptorPool descriptor_pool;

	bool should_shutdown = false;
	bool is_initialised = false;

public:
	VkPipelineLayout pipeline_layout;

public: // vertex buffers
	VkVertexInputBindingDescription binding_description;
	std::vector<VkVertexInputAttributeDescription> attribute_descriptions;
	// VkBuffer vertex_buffer;
	// VkDeviceMemory vertex_buffer_memory;

	GraphicsEngineTexture texture_mgr;

private:
	void createInstance();

	void pick_physical_device();

	void create_logical_device();


	// semaphores and fences, for GPU-GPU and CPU-GPU synchronisation
	// void create_synchronisation_objects(); // moved to swap_chain

// if confused about the different vulkan definitions see here
// https://stackoverflow.com/questions/39557141/what-is-the-difference-between-framebuffer-and-image-in-vulkan

public: // swap chain
	// extent = resolution of the swap chain images and ~ resolution of window we are drawing to
	VkExtent2D choose_swap_extent(const VkSurfaceCapabilitiesKHR& capabilities);

	void recreate_swap_chain(); // useful for when size of window is changing

public: // image views
	void create_image_views();

public: // graphics pipeline
	void create_graphics_pipeline();

public: // frame buffer
	// void create_frame_buffers();

public: // command buffer
	// void create_command_pool();
	VkCommandBuffer begin_single_time_commands();
	void end_single_time_commands(VkCommandBuffer command_buffer);

public: // vertex buffer
	void create_vertex_buffer(Object& object);

	void create_buffer(size_t size, 
					   VkBufferUsageFlags usage_flags, 
					   VkMemoryPropertyFlags memory_flags, 
					   VkBuffer& buffer, 
					   VkDeviceMemory& device_memory);

	void copy_buffer(VkBuffer src_buffer, VkBuffer dest_buffer, size_t size);

	// void update_uniform_buffer(uint32_t current_image);

	void create_descriptor_pools();

	// void create_descriptor_sets(); // todo delete me

	bool bPhysicalDevicePropertiesCached = false;
	VkPhysicalDeviceProperties physical_device_properties;
	const VkPhysicalDeviceProperties& get_physical_device_properties();

private: //uniform buffer
	void create_descriptor_set_layout();
	// void create_uniform_buffers();

public: // other
	// graphics cards offer different types of memory to allocate from, each type of memory varies
	// in therms of allowed operations and performance characteristics
	int find_memory_type(uint32_t type_filter, VkMemoryPropertyFlags flags);
	QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);

public: // main
	void spawn_object(Object& obj);

	// void draw_frame();
};

