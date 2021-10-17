#pragma once

#include "graphics_engine_base_module.hpp"

#include <vulkan/vulkan.hpp>


class GraphicsEngine;
class GraphicsEngineSwapChain;
class GraphicsEngineObject;

// Note that frame refers to swap_chain frame and not actual frames
class GraphicsEngineFrame : GraphicsEngineBaseModule
{
public:
	GraphicsEngineFrame(GraphicsEngine& engine) = delete;
	GraphicsEngineFrame(GraphicsEngine& engine, GraphicsEngineSwapChain& parent_swapchain, VkImage image);
	~GraphicsEngineFrame();

public:
	void spawn_object(GraphicsEngineObject& object);
	void draw();

private:
	void create_descriptor_sets(GraphicsEngineObject& object);
	void create_command_buffer(GraphicsEngineObject& object);
	void update_uniform_buffer();
	void create_synchronisation_objects();

public:
	VkImage image;
	VkImageView image_view;
	VkFramebuffer frame_buffer;

	// descriptors
	std::vector<VkDescriptorSet> descriptor_sets;

	VkCommandBuffer command_buffer;

	uint32_t image_index;

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
};