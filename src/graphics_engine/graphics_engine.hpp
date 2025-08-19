#pragma once

#include "engine_base.hpp"
#include "constants.hpp"
#include "graphics_engine_validation_layer.hpp"
#include "graphics_engine_swap_chain.hpp"
#include "graphics_engine_instance.hpp"
#include "graphics_engine_device.hpp"
#include "resource_manager/graphics_resource_manager.hpp"
#include "graphics_engine_commands.hpp"
#include "graphics_engine_object.hpp"
#include "graphics_engine_texture_manager.hpp"
#include "graphics_engine_gui_manager.hpp"
#include "pipeline/pipeline_manager.hpp"
#include "renderers/renderer_manager.hpp"
#include "raytracing.hpp"
#include "vulkan_wrappers.hpp"
#include "window.hpp"
#include "shared_data_structures.hpp"
#include "queues.hpp"

#include <vulkan/vulkan.hpp>

#include <vector>
#include <mutex>
#include <queue>
#include <unordered_set>


class Analytics;
class Camera;
class GraphicsEngineObject;
class ECS;

class GraphicsEngine : public GraphicsEngineBase
{
public:
	GraphicsEngine() = delete;
	GraphicsEngine(GameEngine& game_engine);
	virtual ~GraphicsEngine() override;

	void run() final;
	void shutdown() { should_shutdown = true; }

public:
	bool is_wireframe_mode = false;

public: // getters and setters
	VkExtent2D get_extent();
	App::Window& get_window();
	Camera* get_camera();
	std::unordered_map<ObjectID, std::unique_ptr<GraphicsEngineObject>>& get_objects() 
	{ 
		return objects; 
	}
	std::unordered_map<ObjectID, GraphicsEngineObject*>& get_offscreen_rendering_objects() 
	{ 
		return offscreen_rendering_objects; 
	}
	GraphicsEngineObject& get_object(ObjectID id) { return *objects.at(id); }
	auto& get_stenciled_object_ids() { return stenciled_objects; }
	VkDevice& get_logical_device() { return device.get_logical_device(); }
	VkPhysicalDevice& get_physical_device() { return device.get_physical_device(); }
	VkInstance& get_instance() { return instance.get(); }
	VkQueue& get_present_queue() { return present_queue; }
	VkQueue& get_graphics_queue() { return graphics_queue; }
	VkSurfaceKHR& get_window_surface() { return instance.window_surface; }
	GraphicsEngineSwapChain& get_swap_chain() { return swap_chain; }
	uint32_t get_num_swapchain_images() { return swap_chain.get_num_images(); }
	VkCommandPool& get_command_pool() { return get_rsrc_mgr().get_command_pool(); }
	GraphicsEnginePipelineManager& get_pipeline_mgr() { return pipeline_mgr; }
	GraphicsEngineTextureManager& get_texture_mgr() { return texture_mgr; }
	GraphicsEngineRayTracing& get_raytracing_module() { return raytracing_component; }
	GraphicsEngineGuiManager& get_graphics_gui_manager() { return gui_manager; }
	GuiManager& get_gui_manager() final { return static_cast<GuiManager&>(gui_manager); }
	RendererManager& get_renderer_mgr() { return renderer_mgr; }
	GraphicsResourceManager& get_rsrc_mgr() { return rsrc_mgr; }
	const GraphicsResourceManager& get_rsrc_mgr() const { return rsrc_mgr; }
	GraphicsEngineDevice& get_device_module() { return device; }
	void set_fps(const float fps) { this->fps = fps; }
	float get_fps() const final { return fps; }
	static constexpr VkSampleCountFlagBits get_msaa_samples() { return VK_SAMPLE_COUNT_4_BIT; }
	ECS& get_ecs();
	const ECS& get_ecs() const;
	uint64_t get_num_objs_deleted() const final { return num_objs_deleted; }
	void increment_num_objs_deleted() final { ++num_objs_deleted; }
	void cleanup_entity(const ObjectID id);

private:
	bool should_shutdown = false;
	VkQueue graphics_queue;
	VkQueue present_queue;
	std::unordered_map<ObjectID, std::unique_ptr<GraphicsEngineObject>> objects;
	std::unordered_set<ObjectID> stenciled_objects;
	// currently used for OffscreenGuiViewportRenderer, in future we should have a scene system
	// and this would be a separate scene
	std::unordered_map<ObjectID, GraphicsEngineObject*> offscreen_rendering_objects;
	std::mutex ge_cmd_q_mutex; // TODO when this becomes a performance bottleneck, we should swap this for a Single Producer Single Producer Lock-Free Queue
	std::queue<std::unique_ptr<GraphicsEngineCommand>> ge_cmd_q;
	std::unique_ptr<Analytics> FPS_tracker;
	std::optional<VkFormat> depth_format;
	float fps = 0.0f;
	uint64_t num_objs_deleted = 0; // used for synchronisation, and knowing when an obj is safe to delete in game engine

// if confused about the different vulkan definitions see here
// https://stackoverflow.com/questions/39557141/what-is-the-difference-between-framebuffer-and-image-in-vulkan

public: // swap chain
	void recreate_swap_chain(); // useful for when size of window is changing
	void update_command_buffer();
	VkCommandBuffer begin_single_time_commands();
	void end_single_time_commands(VkCommandBuffer command_buffer);

	// utilizes vkQueueWaitIdle to ensure that once the function returns, the data is copied into the staging buffer
	void copy_buffer(VkBuffer src_buffer, VkBuffer dest_buffer, size_t size);

	// an image is an actual piece of data memory, similar to a buffer
	void create_image(uint32_t width, 
					  uint32_t height, 
					  VkFormat format,
					  VkImageTiling tiling,
					  VkImageUsageFlags usage,
					  VkMemoryPropertyFlags properties,
					  VkImage& image,
					  VkDeviceMemory& image_memory,
					  VkSampleCountFlagBits num_samples=VkSampleCountFlagBits::VK_SAMPLE_COUNT_1_BIT,
					  const uint32_t layer_count = 1, // for creating a cube map
					  const VkImageCreateFlags flags = 0); // for creating a cube map

	// an image view is just a view of an image, it does not mutate the image
	// i.e. string_view vs string
	VkImageView create_image_view(VkImage& image,
								  VkFormat format,
						   		  VkImageAspectFlags aspect_flags,
								  VkImageViewType view_type = VkImageViewType::VK_IMAGE_VIEW_TYPE_2D,
								  const uint32_t layer_count = 1); // for cube map

	void transition_image_layout(
		VkImage image, 
		VkImageLayout old_layout, 
		VkImageLayout new_layout, 
		VkCommandBuffer command_buffer = nullptr,
		const uint32_t layer_count = 1); // for cubemaps
								  
	VkFormat find_depth_format();

public: // other
	// graphics cards offer different types of memory to allocate from, each type of memory varies
	// in therms of allowed operations and performance characteristics
	int find_memory_type(uint32_t type_filter, VkMemoryPropertyFlags flags);
	QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);

public: // thread safe
	void enqueue_cmd(std::unique_ptr<GraphicsEngineCommand>&& cmd) final;

private: // core components
	GameEngine& game_engine;
	GraphicsEngineInstance instance;
	GraphicsEngineValidationLayer validation_layer;
	GraphicsEngineDevice device;
	GraphicsEngineTextureManager texture_mgr;
	GraphicsResourceManager rsrc_mgr;
	RendererManager renderer_mgr;
	GraphicsEngineSwapChain swap_chain;
	GraphicsEnginePipelineManager pipeline_mgr;
	GraphicsEngineRayTracing raytracing_component;
	GraphicsEngineGuiManager gui_manager;

public: // commands
	void handle_command(SpawnObjectCmd& cmd) final;
	void handle_command(DeleteObjectCmd& cmd) final;
	void handle_command(StencilObjectCmd& cmd) final;
	void handle_command(UnStencilObjectCmd& cmd) final;
	void handle_command(ShutdownCmd& cmd) final;
	void handle_command(ToggleWireFrameModeCmd& cmd) final;
	void handle_command(UpdateCommandBufferCmd& cmd) final;
	void handle_command(UpdateRayTracingCmd& cmd) final;
	void handle_command(PreviewObjectsCmd& cmd) final;
	void handle_command(DestroyResourcesCmd& cmd) final;

private:
	void spawn_object_create_buffers(GraphicsEngineObject& obj);
	void spawn_object_create_dsets(GraphicsEngineObject& obj);
};