#pragma once

#include "graphics_engine_base_module.hpp"
#include "identifications.hpp"
#include "analytics.hpp"

#include <vulkan/vulkan.hpp>

#include <queue>
#include <optional>
#include <filesystem>


class GraphicsEngineSwapChain;

class GraphicsEngineObject;

// Note that frame refers to swap_chain frame and not actual frames
class GraphicsEngineFrame : public GraphicsEngineBaseModule
{
public:
	GraphicsEngineFrame(GraphicsEngine& engine, GraphicsEngineSwapChain& parent_swapchain, VkImage image);
	GraphicsEngineFrame(const GraphicsEngineFrame&) = delete;
	GraphicsEngineFrame(GraphicsEngineFrame&& frame) noexcept;
	~GraphicsEngineFrame();

public:
	void update_command_buffer();
	void draw();

private:
	void update_uniform_buffer();
	void create_synchronisation_objects();
	void maybe_prepare_screenshot_capture();
	void flush_screenshot_capture();
	
	// this function does stuff that needs to be done before cmd buffer is recorded
	// i.e. check for objects to be deleted
	void pre_cmdbuffer_recording();

public:
	// Interprets a VkImage and describes how to access it
	VkImage presentation_image;
	VkImageView presentation_image_view;
	VkCommandBuffer command_buffer;

	const uint32_t image_index;
	
	// delete object
	void mark_obj_for_delete(ObjectID id);

private:
	
	GraphicsEngineSwapChain& swap_chain;

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

	std::queue<ObjectID> objs_to_delete;
	std::optional<GraphicsBuffer> screenshot_staging_buffer;
	std::filesystem::path screenshot_path;
	VkExtent2D screenshot_extent{};
};
