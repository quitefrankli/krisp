#pragma once

#include "graphics_engine_frame.hpp"

#include "graphics_engine.hpp"
#include "graphics_engine_swap_chain.hpp"
#include "shared_data_structures.hpp"
#include "pipeline/pipeline.hpp"
#include "renderable/render_types.hpp"

#include <glm/gtx/string_cast.hpp>

#include <array>
#include <iostream>
#include <algorithm>


int GraphicsEngineFrame::global_image_index = 0;

GraphicsEngineFrame::GraphicsEngineFrame(
	GraphicsEngine& engine, 
	GraphicsEngineSwapChain& parent_swapchain, 
	VkImage presentation_image) :
	GraphicsEngineBaseModule(engine),
	presentation_image(presentation_image),
	swap_chain(parent_swapchain),
	image_index(global_image_index++),
	analytics(std::string("Frame ") + std::to_string(image_index), 60)
{
	presentation_image_view = get_graphics_engine().create_image_view(
		presentation_image, 
		swap_chain.get_image_format(), 
		VK_IMAGE_ASPECT_COLOR_BIT);
	for (auto& [_, renderer] : engine.get_renderer_mgr().get_renderers())
	{
		renderer->allocate_per_frame_resources(presentation_image, presentation_image_view);
	}

	create_synchronisation_objects();
	command_buffer = get_rsrc_mgr().create_command_buffer();
}

GraphicsEngineFrame::GraphicsEngineFrame(GraphicsEngineFrame&& frame) noexcept :
	GraphicsEngineBaseModule(frame.get_graphics_engine()),
	presentation_image(std::move(frame.presentation_image)),
	presentation_image_view(std::move(frame.presentation_image_view)),
	command_buffer(std::move(frame.command_buffer)),
	image_index(std::move(frame.image_index)),
	swap_chain(frame.swap_chain),
	image_available_semaphore(std::move(frame.image_available_semaphore)),
	render_finished_semaphore(std::move(frame.render_finished_semaphore)),
	fence_frame_inflight(std::move(frame.fence_frame_inflight)),
	fence_image_inflight(std::move(frame.fence_image_inflight)),
	submission_serial(std::move(frame.submission_serial)),
	analytics(std::move(frame.analytics)),
	screenshot_staging_buffer(std::move(frame.screenshot_staging_buffer)),
	screenshot_path(std::move(frame.screenshot_path)),
	screenshot_extent(std::move(frame.screenshot_extent)),
	recording_staging_buffer(std::move(frame.recording_staging_buffer)),
	recording_extent(std::move(frame.recording_extent)),
	recording_has_pending_frame(frame.recording_has_pending_frame),
	recording_capture_target(std::move(frame.recording_capture_target))
{
	frame.should_destroy = false;
}

GraphicsEngineFrame::~GraphicsEngineFrame()
{
	if (!this->should_destroy)
	{
		return;
	}

	global_image_index--;
	vkFreeCommandBuffers(get_logical_device(), get_graphics_engine().get_command_pool(), 1, &command_buffer);

	// cleanup synchronisation objects
	vkDestroySemaphore(get_logical_device(), image_available_semaphore, nullptr);
	vkDestroySemaphore(get_logical_device(), render_finished_semaphore, nullptr);
	vkDestroyFence(get_logical_device(), fence_frame_inflight, nullptr);
	// vkDestroyFence(get_logical_device(), fence_image_inflight, nullptr); // this isn't an actual fence it's rather a reference to the inflight frame fence
	vkDestroyImageView(get_logical_device(), presentation_image_view, nullptr);
	if (screenshot_staging_buffer.has_value())
	{
		screenshot_staging_buffer->destroy(get_logical_device());
	}
	if (recording_staging_buffer.has_value())
	{
		recording_staging_buffer->destroy(get_logical_device());
	}
}

void GraphicsEngineFrame::update_command_buffer()
{
	VkCommandBufferResetFlags reset_flags = 0;
	vkResetCommandBuffer(command_buffer, reset_flags);

	// starting command buffer recording
	VkCommandBufferBeginInfo begin_info{};
	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin_info.flags = 0;
	begin_info.pInheritanceInfo = nullptr;

	if (vkBeginCommandBuffer(command_buffer, &begin_info) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to begin recording command buffer!");
	}

	auto& renderer_mgr = get_graphics_engine().get_renderer_mgr();
	const auto submit_draw_commands = [&](const ERendererType renderer_type)
	{
		renderer_mgr.get_renderer(renderer_type).submit_draw_commands(command_buffer, presentation_image_view, image_index);
	};
	get_graphics_engine().get_texture_compositor().record_pending(command_buffer);
	// Ray tracing is unsupported:
	// if (get_graphics_engine().get_render_mode() == ERenderMode::RAYTRACING)
	//     submit_draw_commands(ERendererType::RAYTRACING);
	if (get_graphics_engine().get_render_mode() != ERenderMode::UNLIT_BASE_COLOR)
		submit_draw_commands(ERendererType::SHADOW_MAP);
	submit_draw_commands(ERendererType::RASTERIZATION);
	submit_draw_commands(ERendererType::PRESENTATION);
	// Note: Particle rendering is now done within the RasterizationRenderer
	submit_draw_commands(ERendererType::QUAD);
	maybe_prepare_screenshot_capture();

	submit_draw_commands(ERendererType::GUI); // ImGui is composed after the SDR screenshot copy.

	maybe_prepare_recording_capture();
	
	if (vkEndCommandBuffer(command_buffer) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to record command buffer!");
	}
}

void GraphicsEngineFrame::draw()
{
	// 1. acquire image from swap chain
	// 2. execute command buffer with image as attachment in the frame buffer
	// 3. return the image to swap chain for presentation

	analytics.start();
	// CPU-GPU synchronisation for in flight images
	if (vkWaitForFences(
			get_logical_device(), 1, &fence_frame_inflight, VK_TRUE,
			std::numeric_limits<uint64_t>::max()) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to wait for in-flight frame!");
	}
	analytics.stop();
	if (submission_serial)
	{
		get_graphics_engine().complete_graphics_submission(*submission_serial);
		submission_serial.reset();
	}

	update_command_buffer();

	uint32_t swap_chain_image_index;
	// waits until there's an image available to use in the swap chain
	VkResult result = vkAcquireNextImageKHR(get_logical_device(), 
											swap_chain.get_swap_chain(), 
											std::numeric_limits<uint64_t>::max(), // wait time (ns)
											image_available_semaphore, 
											VK_NULL_HANDLE, 
											&swap_chain_image_index); // image index of the available image
	if (image_index != swap_chain_image_index)
	{
		throw std::runtime_error("image_index and swap_chain_image_index mismatch!");
	}											
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("failed to acquire swapchain image!");
	}
	// if (result == VK_ERROR_OUT_OF_DATE_KHR) {
	// 	reset();
	// 	return;
	// } else if (image_index != swap_chain_image_index) {
	// 	throw std::runtime_error("image_index and swap_chain_image_index mismatch!");
	// } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
	// 	throw std::runtime_error("failed to acquire swap chain image!");
	// }

	// check if a previous frame is using this image (i.e. there is a fence to wait on)
	if (fence_image_inflight != VK_NULL_HANDLE)
	{
		vkWaitForFences(get_logical_device(), 1, &fence_image_inflight, VK_TRUE, std::numeric_limits<uint64_t>::max());
	}
	// mark the image as now being in use by this frame
	fence_image_inflight = fence_frame_inflight;

	update_uniform_buffer();

	//
	// submitting the command buffer
	//

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	VkSemaphore waitSemaphores[] = { image_available_semaphore };
	VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
	// here we specify which semaphore to wait on before execution begins and in which stage of the pipeline to wait
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = waitSemaphores;
	submitInfo.pWaitDstStageMask = waitStages;

	// here we specify which command buffers to actually submit for execution
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &command_buffer;

	// here we specify the semaphores to signal once the command buffer has finished execution
	VkSemaphore signal_semaphores[] = { render_finished_semaphore };
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = signal_semaphores;

	vkResetFences(get_logical_device(), 1, &fence_frame_inflight);
	if (vkQueueSubmit(get_graphics_engine().get_graphics_queue(), 1, &submitInfo, fence_frame_inflight) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to submit draw command buffer!");
	}
	submission_serial = get_graphics_engine().register_graphics_submission();

	//
	// Presentation
	// this step submits the result of the swapchain to have it eventually show up on the screen
	//

	VkPresentInfoKHR present_info{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
	present_info.waitSemaphoreCount = 1;
	// the semaphore to wait on before presentation
	present_info.pWaitSemaphores = signal_semaphores;

	present_info.swapchainCount = 1;
	present_info.pSwapchains = &swap_chain.get_swap_chain();
	present_info.pImageIndices = &image_index;
	present_info.pResults = nullptr; // allows you to specify array of VkResult values to check for every individual swap chain if presentation was successful

	result = vkQueuePresentKHR(get_graphics_engine().get_present_queue(), &present_info);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("failed to present swap chain image!");
	}

	flush_screenshot_capture();
	flush_recording_capture();

	// if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || frame_buffer_resized) { // recreate if we resize window
	// 	frame_buffer_resized = false;
	// 	reset();
	// } else if (result != VK_SUCCESS) {
	// 	throw std::runtime_error("failed to present swap chain image!");
	// }
}

void GraphicsEngineFrame::update_uniform_buffer()
{
	// update global uniform buffer
	const RenderFrame& render_frame = get_graphics_engine().get_render_frame();
	SDS::GlobalData gubo{};
	gubo.view = render_frame.camera.view;
	gubo.proj = render_frame.camera.projection;
	gubo.view_pos = render_frame.camera.position;

	// currently uses a single active light source
	if (render_frame.active_light)
	{
		const auto& light = *render_frame.active_light;
		gubo.light_pos = light.position;
		gubo.light_color = light.color;
		gubo.light_intensity = light.intensity;
	}
	else
	{
		gubo.light_pos = glm::vec3(0.0f, 5.0f, 0.0f);
		gubo.light_color = glm::vec3(0.0f);
		gubo.light_intensity = 0.0f;
	}
	gubo.shadow_far_plane = 256.0f;

	const std::array<glm::vec3, 6> SHADOW_DIRECTIONS = {
		Maths::right_vec,
		-Maths::right_vec,
		Maths::up_vec,
		-Maths::up_vec,
		Maths::forward_vec,
		-Maths::forward_vec
	};
	const std::array<glm::vec3, 6> SHADOW_UPS = {
		Maths::up_vec,
		Maths::up_vec,
		-Maths::forward_vec,
		Maths::forward_vec,
		Maths::up_vec,
		Maths::up_vec
	};

	const glm::mat4 shadow_proj = glm::perspectiveLH(
		Maths::deg2rad(90.0f),
		1.0f,
		0.1f,
		gubo.shadow_far_plane);
	for (int face_idx = 0; face_idx < 6; ++face_idx)
	{
		const glm::mat4 shadow_view = glm::lookAtLH(
			gubo.light_pos,
			gubo.light_pos + SHADOW_DIRECTIONS[face_idx],
			SHADOW_UPS[face_idx]);
		gubo.shadow_view_proj_mats[face_idx] = shadow_proj * shadow_view;
	}

	get_rsrc_mgr().write_to_global_uniform_buffer(image_index, gubo);

	// Update the producer-composed transform for each renderable.
	SDS::ObjectData object_data{};
	for (const auto& [_, graphics_renderable] : get_graphics_engine().get_renderables())
	{
		object_data.model = graphics_renderable->get_model_transform();
		object_data.mvp = gubo.proj * gubo.view * object_data.model;
		object_data.rot_mat = glm::mat3(object_data.model);
		get_rsrc_mgr().write_to_buffer(
			RenderableFrameID{graphics_renderable->get_id(), image_index},
			object_data);
	}

	// Skeleton pose resources are shared by every bound renderable and updated once.
	for (const auto& pose : render_frame.skeletons)
	{
		std::vector<SDS::Bone> bones =
			compose_bone_transforms(pose.local_transforms, *pose.definition);
		get_rsrc_mgr().write_to_buffer(
			SkeletonFrameID(pose.definition->id, image_index),
			bones);
	}
}

void GraphicsEngineFrame::create_synchronisation_objects()
{
	VkSemaphoreCreateInfo semaphore_create_info{};
	semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo fence_create_info{};
	fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fence_create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	if (vkCreateSemaphore(get_logical_device(), &semaphore_create_info, nullptr, &image_available_semaphore) != VK_SUCCESS ||
		vkCreateSemaphore(get_logical_device(), &semaphore_create_info, nullptr, &render_finished_semaphore) != VK_SUCCESS ||
		vkCreateFence(get_logical_device(), &fence_create_info, nullptr, &fence_frame_inflight) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create semaphores!");
	}
}
