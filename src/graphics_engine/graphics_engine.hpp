#pragma once

#include "graphics_engine_validation_layer.hpp"
#include "graphics_engine_swap_chain.hpp"
#include "graphics_engine_instance.hpp"
#include "graphics_engine_device.hpp"
#include "graphics_resource_manager.hpp"
#include "graphics_engine_commands.hpp"
#include "graphics_engine_object.hpp"
#include "graphics_engine_depth_buffer.hpp"
#include "graphics_engine_texture_manager.hpp"
#include "graphics_engine_gui_manager.hpp"
#include "pipeline/pipeline_manager.hpp"

#include "vertex.hpp"
#include "queues.hpp"

#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>

#include <vector>
#include <mutex>
#include <queue>


class Analytics;
class Camera;
class GameEngine;
class GraphicsEngineObject;
class LightSource;

class GraphicsEngine
{
public:
	GraphicsEngine() = delete;
	GraphicsEngine(GameEngine& _game_engine);
	~GraphicsEngine();

	void run();
	void shutdown() { should_shutdown = true; }

	const VkVertexInputBindingDescription binding_description;
	const std::vector<VkVertexInputAttributeDescription> attribute_descriptions;

public:
	bool is_wireframe_mode = false;

public: // getters and setters
	template<class T = int> 
	T get_window_width() const { return static_cast<T>(swap_chain.get_extent().width); }
	template<class T = int>
	T get_window_height() const { return static_cast<T>(swap_chain.get_extent().height); }
	VkExtent2D get_extent_unsafe();
	GLFWwindow* get_window();
	Camera* get_camera();
	void add_vertex_set(const std::vector<Vertex>& vertex_set) { vertex_sets.emplace_back(vertex_set); }
	std::vector<std::vector<Vertex>>& get_vertex_sets();
	void insert_object(Object* object);
	auto& get_objects() { return objects; }
	auto& get_light_sources() { return light_sources; }
	VkDevice& get_logical_device() { return device.get_logical_device(); }
	VkPhysicalDevice& get_physical_device() { return device.get_physical_device(); }
	VkInstance& get_instance() { return instance.get(); }
	VkQueue& get_present_queue() { return present_queue; }
	VkQueue& get_graphics_queue() { return graphics_queue; }
	VkSurfaceKHR& get_window_surface() { return instance.window_surface; }
	GraphicsEngineSwapChain& get_swap_chain() { return swap_chain; }
	uint32_t get_num_swapchain_images() const { return swap_chain.get_num_images(); }
	VkDescriptorPool& get_descriptor_pool() { return pool.descriptor_pool; }
	VkCommandPool& get_command_pool() { return pool.get_command_pool(); }
	GraphicsEnginePipelineManager& get_pipeline_mgr() { return pipeline_mgr; }
	GraphicsEngineDepthBuffer& get_depth_buffer() { return depth_buffer; }
	GraphicsEngineTextureManager& get_texture_mgr() { return texture_mgr; }
	GraphicsEngineGuiManager& get_gui_manager() { return gui_manager; }
	VkBuffer& get_global_uniform_buffer() { return pool.global_uniform_buffer; }
	VkDeviceMemory& get_global_uniform_buffer_memory() { return pool.global_uniform_buffer_memory; }
	GraphicsResourceManager& get_graphics_resource_manager() { return pool; }
	GraphicsEngineDevice& get_device_module() { return device; }

private: // flags (these must be before core components)
	bool should_shutdown = false;

private: // core components
	GameEngine& game_engine;
	GraphicsEngineInstance instance;
	GraphicsEngineValidationLayer validation_layer;
	GraphicsEngineDevice device;
	GraphicsEngineTextureManager texture_mgr;
	GraphicsResourceManager pool;
	GraphicsEngineDepthBuffer depth_buffer;
	GraphicsEngineSwapChain swap_chain;
	GraphicsEnginePipelineManager pipeline_mgr;
	GraphicsEngineGuiManager gui_manager;

private:
	VkQueue graphics_queue;
	VkQueue present_queue;
	std::vector<std::vector<Vertex>> vertex_sets;
	std::unordered_map<uint64_t, std::unique_ptr<GraphicsEngineObject>> objects;
	std::unordered_map<uint64_t, std::reference_wrapper<const LightSource>> light_sources;
	std::mutex ge_cmd_q_mutex; // TODO when this becomes a performance bottleneck, we should swap this for a Single Producer Single Producer Lock-Free Queue
	std::queue<std::unique_ptr<GraphicsEngineCommand>> ge_cmd_q;

	std::unique_ptr<Analytics> FPS_tracker;

// if confused about the different vulkan definitions see here
// https://stackoverflow.com/questions/39557141/what-is-the-difference-between-framebuffer-and-image-in-vulkan

public: // swap chain
	// extent = resolution of the swap chain images and ~ resolution of window we are drawing to
	VkExtent2D choose_swap_extent(const VkSurfaceCapabilitiesKHR& capabilities);
	void recreate_swap_chain(); // useful for when size of window is changing

public: // command buffer
	void update_command_buffer();
	VkCommandBuffer begin_single_time_commands();
	void end_single_time_commands(VkCommandBuffer command_buffer);

public: // buffers
	void create_object_buffers(GraphicsEngineObject& object);
	
public:
	void create_buffer(size_t size, 
					   VkBufferUsageFlags usage_flags, 
					   VkMemoryPropertyFlags memory_flags, 
					   VkBuffer& buffer, 
					   VkDeviceMemory& device_memory);
	void copy_buffer(VkBuffer src_buffer, VkBuffer dest_buffer, size_t size);

	// an image is an actual piece of data memory, similar to a buffer
	void create_image(uint32_t width, 
					  uint32_t height, 
					  VkFormat format, 
					  VkImageTiling tiling,
					  VkImageUsageFlags usage,
					  VkMemoryPropertyFlags properties,
					  VkImage& image,
					  VkDeviceMemory& image_memory);

	// an image view is just a view of an image, it does not mutate the image
	// i.e. string_view vs string
	VkImageView create_image_view(VkImage& image,
								  VkFormat format,
						   		  VkImageAspectFlags aspect_flags,
								  VkImageViewType view_type = VkImageViewType::VK_IMAGE_VIEW_TYPE_2D);
								  
public: // other
	// graphics cards offer different types of memory to allocate from, each type of memory varies
	// in therms of allowed operations and performance characteristics
	int find_memory_type(uint32_t type_filter, VkMemoryPropertyFlags flags);
	QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);

public: // thread safe
	void enqueue_cmd(std::unique_ptr<GraphicsEngineCommand>&& cmd);

private: // friends
	friend SpawnObjectCmd;
	friend DeleteObjectCmd;
};