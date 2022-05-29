#pragma once

#include "graphics_engine_base_module.hpp"

#include "analytics.hpp"

#include <vulkan/vulkan.hpp>

#include <queue>


template<typename GraphicsEngineT>
class GraphicsEngineSwapChain;

template<typename GraphicsEngineT>
class GraphicsEngineObject;

// Note that frame refers to swap_chain frame and not actual frames
template<typename GraphicsEngineT>
class GraphicsEngineFrame : public GraphicsEngineBaseModule<GraphicsEngineT>
{
public:
	GraphicsEngineFrame(GraphicsEngineT& engine, GraphicsEngineSwapChain<GraphicsEngineT>& parent_swapchain, VkImage image);
	GraphicsEngineFrame(const GraphicsEngineFrame&) = delete;
	GraphicsEngineFrame(GraphicsEngineFrame&& frame) noexcept;
	~GraphicsEngineFrame();

public:
	void spawn_object(GraphicsEngineObject<GraphicsEngineT>& object);
	void update_command_buffer();
	void draw();

private:
	void create_descriptor_sets(GraphicsEngineObject<GraphicsEngineT>& object);
	void create_command_buffer();
	void update_uniform_buffer();
	void create_synchronisation_objects();
	
	// this function does stuff that needs to be done before cmd buffer is recorded
	// i.e. check for objects to be deleted
	void pre_cmdbuffer_recording();

public:
	VkImage image;
	VkImageView image_view;
	VkFramebuffer frame_buffer;

	VkCommandBuffer command_buffer;

	uint32_t image_index;
	
	// delete object
	void mark_obj_for_delete(uint64_t id);

private:
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_graphics_engine;
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_logical_device;
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_physical_device;
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_instance;
	using GraphicsEngineBaseModule<GraphicsEngineT>::create_buffer;
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_num_swapchain_frames;
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_render_pass;
	using GraphicsEngineBaseModule<GraphicsEngineT>::should_destroy;
	
	GraphicsEngineSwapChain<GraphicsEngineT>& swap_chain;

	//
	// Image refers to an image in a swap chain
	// Frame refers to a "canvas" being processed (i.e. by GPU) we can think of frames as being in-flight
	// We produce a frame from a swapchain image and return it back to the image for presentation oncec we
	//  are done processing it.
	//

	// GPU-GPU synchronisation
	VkSemaphore image_available_semaphore;
	VkSemaphore render_finished_semaphore;

	// CPU-GPU synchronisation
	// these track for each swap chain image and checks if an inflight frame is currently using it
	VkFence fence_frame_inflight = VK_NULL_HANDLE; // signals when the command buffer finishes executing or when an inflight frame is finished
	VkFence fence_image_inflight = VK_NULL_HANDLE; // check if a frame in flight is already using a swapchain image 

	static int global_image_index;

	Analytics analytics;

	std::queue<uint64_t> objs_to_delete;
};