#pragma once

#include "engine_base.hpp"
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
#include "vertex.hpp"
#include "queues.hpp"

#include <vulkan/vulkan.hpp>

#include <vector>
#include <mutex>
#include <queue>
#include <unordered_set>


class Analytics;
class Camera;
template<typename GraphicsEngineT>
class GraphicsEngineObject;
class LightSource;

template<typename GameEngineT>
class GraphicsEngine : public GraphicsEngineBase
{
public:
	GraphicsEngine() = delete;
	GraphicsEngine(GameEngineT& game_engine);
	~GraphicsEngine();

	void run();
	void shutdown() { should_shutdown = true; }

	const VkVertexInputBindingDescription binding_description;
	const std::vector<VkVertexInputAttributeDescription> attribute_descriptions;

public:
	bool is_wireframe_mode = false;

public: // getters and setters
	VkExtent2D get_extent();
	App::Window& get_window();
	Camera* get_camera();
	void add_vertex_set(const std::vector<Vertex>& vertex_set) { vertex_sets.emplace_back(vertex_set); }
	std::vector<std::vector<Vertex>>& get_vertex_sets();
	void insert_object(Object* object);
	std::unordered_map<ObjectID, std::unique_ptr<GraphicsEngineObject<GraphicsEngine>>>& get_objects() 
	{ 
		return objects; 
	}
	std::unordered_map<ObjectID, GraphicsEngineObject<GraphicsEngine>*>& get_offscreen_rendering_objects() 
	{ 
		return offscreen_rendering_objects; 
	}
	GraphicsEngineObject<GraphicsEngine>& get_object(ObjectID id) { return *objects.at(id); }
	auto& get_stenciled_object_ids() { return stenciled_objects; }
	auto& get_light_sources() { return light_sources; }
	VkDevice& get_logical_device() { return device.get_logical_device(); }
	VkPhysicalDevice& get_physical_device() { return device.get_physical_device(); }
	VkInstance& get_instance() { return instance.get(); }
	VkQueue& get_present_queue() { return present_queue; }
	VkQueue& get_graphics_queue() { return graphics_queue; }
	VkSurfaceKHR& get_window_surface() { return instance.window_surface; }
	GraphicsEngineSwapChain<GraphicsEngine>& get_swap_chain() { return swap_chain; }
	static constexpr uint32_t get_num_swapchain_images()
	{ 
		return GraphicsEngineSwapChain<GraphicsEngine>::EXPECTED_NUM_SWAPCHAIN_IMAGES; 
	}
	VkDescriptorPool& get_descriptor_pool() { return get_rsrc_mgr().descriptor_pool; }
	VkCommandPool& get_command_pool() { return get_rsrc_mgr().get_command_pool(); }
	GraphicsEnginePipelineManager<GraphicsEngine>& get_pipeline_mgr() { return pipeline_mgr; }
	GraphicsEngineTextureManager<GraphicsEngine>& get_texture_mgr() { return texture_mgr; }
	GraphicsEngineRayTracing<GraphicsEngine>& get_raytracing_module() { return raytracing_component; }
	GraphicsEngineGuiManager<GraphicsEngine, GameEngineT>& get_graphics_gui_manager() { return gui_manager; }
	GuiManager<GameEngineT>& get_gui_manager() { return static_cast<GuiManager<GameEngineT>&>(gui_manager); }
	RendererManager<GraphicsEngine>& get_renderer_mgr() { return renderer_mgr; }
	GraphicsResourceManager<GraphicsEngine>& get_rsrc_mgr() { return rsrc_mgr; }
	GraphicsEngineDevice<GraphicsEngine>& get_device_module() { return device; }
	void set_fps(const float fps) { this->fps = fps; }
	float get_fps() const { return fps; }
	static constexpr VkSampleCountFlagBits get_msaa_samples() { return VK_SAMPLE_COUNT_4_BIT; }

private:
	bool should_shutdown = false;
	VkQueue graphics_queue;
	VkQueue present_queue;
	std::vector<std::vector<Vertex>> vertex_sets;
	std::unordered_map<ObjectID, std::unique_ptr<GraphicsEngineObject<GraphicsEngine>>> objects;
	std::unordered_map<ObjectID, std::reference_wrapper<const LightSource>> light_sources;
	std::unordered_set<ObjectID> stenciled_objects;
	// currently used for OffscreenGuiViewportRenderer, in future we should have a scene system
	// and this would be a separate scene
	std::unordered_map<ObjectID, GraphicsEngineObject<GraphicsEngine>*> offscreen_rendering_objects;
	std::mutex ge_cmd_q_mutex; // TODO when this becomes a performance bottleneck, we should swap this for a Single Producer Single Producer Lock-Free Queue
	std::queue<std::unique_ptr<GraphicsEngineCommand>> ge_cmd_q;
	std::unique_ptr<Analytics> FPS_tracker;
	std::optional<VkFormat> depth_format;
	float fps = 0.0f;

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
					  VkSampleCountFlagBits num_samples=VkSampleCountFlagBits::VK_SAMPLE_COUNT_1_BIT);

	// an image view is just a view of an image, it does not mutate the image
	// i.e. string_view vs string
	VkImageView create_image_view(VkImage& image,
								  VkFormat format,
						   		  VkImageAspectFlags aspect_flags,
								  VkImageViewType view_type = VkImageViewType::VK_IMAGE_VIEW_TYPE_2D);

	void transition_image_layout(
		VkImage image, 
		VkImageLayout old_layout, 
		VkImageLayout new_layout, 
		VkCommandBuffer command_buffer = nullptr);
								  
	VkFormat find_depth_format();

public: // other
	// graphics cards offer different types of memory to allocate from, each type of memory varies
	// in therms of allowed operations and performance characteristics
	int find_memory_type(uint32_t type_filter, VkMemoryPropertyFlags flags);
	QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);

public: // thread safe
	void enqueue_cmd(std::unique_ptr<GraphicsEngineCommand>&& cmd);

private: // core components
	GameEngineT& game_engine;
	GraphicsEngineInstance<GraphicsEngine> instance;
	GraphicsEngineValidationLayer<GraphicsEngine> validation_layer;
	GraphicsEngineDevice<GraphicsEngine> device;
	GraphicsEngineTextureManager<GraphicsEngine> texture_mgr;
	GraphicsResourceManager<GraphicsEngine> rsrc_mgr;
	RendererManager<GraphicsEngine> renderer_mgr;
	GraphicsEngineSwapChain<GraphicsEngine> swap_chain;
	GraphicsEnginePipelineManager<GraphicsEngine> pipeline_mgr;
	GraphicsEngineRayTracing<GraphicsEngine> raytracing_component;
	GraphicsEngineGuiManager<GraphicsEngine, GameEngineT> gui_manager;

public: // commands
	void handle_command(SpawnObjectCmd& cmd) final;
	void handle_command(AddLightSourceCmd& cmd) final;
	void handle_command(DeleteObjectCmd& cmd) final;
	void handle_command(StencilObjectCmd& cmd) final;
	void handle_command(UnStencilObjectCmd& cmd) final;
	void handle_command(ShutdownCmd& cmd) final;
	void handle_command(ToggleWireFrameModeCmd& cmd) final;
	void handle_command(UpdateCommandBufferCmd& cmd) final;
	void handle_command(UpdateRayTracingCmd& cmd) final;
	void handle_command(PreviewObjectsCmd& cmd) final;

private:
	static VkVertexInputBindingDescription get_binding_description();
	static std::vector<VkVertexInputAttributeDescription> get_attribute_descriptions();
};