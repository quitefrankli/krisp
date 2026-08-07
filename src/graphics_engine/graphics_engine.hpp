#pragma once

#include "engine_base.hpp"
#include "constants.hpp"
#include "graphics_engine_validation_layer.hpp"
#include "graphics_engine_swap_chain.hpp"
#include "graphics_engine_instance.hpp"
#include "graphics_engine_device.hpp"
#include "resource_manager/graphics_resource_manager.hpp"
#include "graphics_renderable.hpp"
#include "render_draw_list.hpp"
#include "graphics_engine_texture_manager.hpp"
#include "texture_compositor.hpp"
#include "graphics_engine_gui_manager.hpp"
#include "pipeline/pipeline_manager.hpp"
#include "renderers/renderer_manager.hpp"
#include "submission_retirement_queue.hpp"
// #include "raytracing.hpp" // Ray tracing is unsupported.
#include "vulkan_wrappers.hpp"
#include "window.hpp"
#include "shared_data_structures.hpp"
#include "queues.hpp"

#include <vulkan/vulkan.hpp>

#include <atomic>
#include <vector>
#include <unordered_set>
#include <optional>


class Analytics;
class GraphicsRenderable;
class VideoRecorder;

struct GraphicsSkeletonResources
{
	SkeletonID id;
	uint32_t frame_allocation_count;
};

struct RetiredGraphicsResources
{
	std::vector<GraphicsRenderableResources> renderables;
	std::vector<GraphicsSkeletonResources> skeletons;
	std::vector<MaterialID> materials;
	std::vector<MeshID> meshes;

	bool empty() const
	{
		return renderables.empty() && skeletons.empty()
			&& materials.empty() && meshes.empty();
	}
};

class GraphicsEngine : public GraphicsEngineBase
{
public:
	GraphicsEngine() = delete;
	explicit GraphicsEngine(App::Window& window);
	virtual ~GraphicsEngine() override;

	void run() final;
	void request_shutdown() final
	{
		should_shutdown.store(true, std::memory_order_release);
		get_recording_session().stop();
	}

public: // getters and setters
	VkExtent2D get_extent();
	App::Window& get_window();
	std::unordered_map<RenderableID, std::unique_ptr<GraphicsRenderable>>& get_renderables()
	{ 
		return renderables;
	}
	const GraphicsDrawLists& get_draw_lists() const { return draw_lists; }
	GraphicsRenderable& get_renderable(RenderableID id) { return *renderables.at(id); }
	ERenderMode get_render_mode() const { return get_render_frame().view.render_mode; }
	const auto& get_stenciled_object_ids() const
	{
		return get_render_frame().view.stenciled_objects;
	}
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
	TextureCompositor& get_texture_compositor() { return texture_compositor; }
	// GraphicsEngineRayTracing& get_raytracing_module() { return raytracing_component; }
	GraphicsEngineGuiManager& get_graphics_gui_manager() { return gui_manager; }
	EngineUiManager& get_gui_manager() final { return gui_manager.get_engine_ui_manager(); }
	void set_application_ui_manager(ApplicationUiManager* manager) final
	{
		gui_manager.set_application_ui_manager(manager);
	}
	void set_ui_layers_active(bool engine_active, bool application_active) final
	{
		gui_manager.set_ui_layers_active(engine_active, application_active);
	}
	VideoRecorder& get_video_recorder() { return *video_recorder; }
	RendererManager& get_renderer_mgr() { return renderer_mgr; }
	GraphicsResourceManager& get_rsrc_mgr() { return rsrc_mgr; }
	const GraphicsResourceManager& get_rsrc_mgr() const { return rsrc_mgr; }
	GraphicsEngineDevice& get_device_module() { return device; }
	void set_fps(const float fps) { this->fps = fps; }
	float get_fps() const final { return fps; }
	static constexpr VkSampleCountFlagBits get_msaa_samples() { return VK_SAMPLE_COUNT_4_BIT; }
	const RenderFrame& get_render_frame() const { return *accepted_render_frame; }
	const RenderableState& get_renderable_state(RenderableID id) const
	{
		return accepted_render_frame->renderables.at(renderable_indices.at(id));
	}
	const RenderSkeletonPose& get_render_skeleton_pose(SkeletonID id) const
	{
		return accepted_render_frame->skeletons.at(render_skeleton_indices.at(id));
	}
	SubmissionSerial register_graphics_submission();
	void complete_graphics_submission(SubmissionSerial serial);

private:
	std::atomic<bool> should_shutdown = false;
	VkQueue graphics_queue;
	VkQueue present_queue;
	std::unordered_map<RenderableID, std::unique_ptr<GraphicsRenderable>> renderables;
	GraphicsDrawLists draw_lists;
	std::unique_ptr<Analytics> FPS_tracker;
	std::optional<VkFormat> depth_format;
	float fps = 0.0f;

// if confused about the different vulkan definitions see here
// https://stackoverflow.com/questions/39557141/what-is-the-difference-between-framebuffer-and-image-in-vulkan

public: // swap chain
	void recreate_swap_chain(); // useful for when size of window is changing
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
					  const VkImageCreateFlags flags = 0, // for creating a cube map
					  const uint32_t mip_levels = 1);

	// an image view is just a view of an image, it does not mutate the image
	// i.e. string_view vs string
	VkImageView create_image_view(VkImage& image,
								  VkFormat format,
								  VkImageAspectFlags aspect_flags,
								  VkImageViewType view_type = VkImageViewType::VK_IMAGE_VIEW_TYPE_2D,
								  const uint32_t layer_count = 1,
								  const uint32_t mip_levels = 1); // for cube map

	void transition_image_layout(
		VkImage image, 
		VkImageLayout old_layout, 
		VkImageLayout new_layout, 
		VkCommandBuffer command_buffer = nullptr,
		const uint32_t layer_count = 1,
		const uint32_t mip_levels = 1); // for cubemaps
								  
	VkFormat find_depth_format();

public: // other
	// graphics cards offer different types of memory to allocate from, each type of memory varies
	// in therms of allowed operations and performance characteristics
	int find_memory_type(uint32_t type_filter, VkMemoryPropertyFlags flags);
	QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);

private: // core components
	App::Window& window;
	RenderFramePtr accepted_render_frame;
	std::unordered_map<RenderableID, uint32_t> renderable_indices;
	std::unordered_map<SkeletonID, uint32_t> render_skeleton_indices;
	std::unordered_map<SkeletonID, uint32_t> graphics_skeleton_frame_counts;
	SubmissionRetirementQueue<RetiredGraphicsResources> retirement_queue;
	SubmissionSerial last_submitted_serial = 0;
	SubmissionSerial completed_submission_serial = 0;
	GraphicsEngineInstance instance;
	GraphicsEngineValidationLayer validation_layer;
	GraphicsEngineDevice device;
	GraphicsEngineTextureManager texture_mgr;
	GraphicsResourceManager rsrc_mgr;
	RendererManager renderer_mgr;
	GraphicsEngineSwapChain swap_chain;
	GraphicsEnginePipelineManager pipeline_mgr;
	TextureCompositor texture_compositor;
	// GraphicsEngineRayTracing raytracing_component;
	GraphicsEngineGuiManager gui_manager;
	std::unique_ptr<VideoRecorder> video_recorder;

private:
	void accept_latest_render_frame();
	void reconcile_topology(const RenderFrame& frame);
	void retire_unused_resources();
	void enqueue_retirement(RetiredGraphicsResources resources);
	void release_retired_resources(RetiredGraphicsResources resources);
	void create_renderable_buffers(GraphicsRenderable& renderable);
	void create_renderable_dsets(GraphicsRenderable& renderable);
};
