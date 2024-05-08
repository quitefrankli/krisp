#pragma once

#include "graphics_engine_frame.hpp"

#include "graphics_engine.hpp"
#include "graphics_engine_swap_chain.hpp"
#include "objects/object.hpp"
#include "shared_data_structures.hpp"
#include "camera.hpp"
#include "pipeline/pipeline.hpp"
#include "renderable/render_types.hpp"

#include "entity_component_system/ecs.hpp"

#include <glm/gtx/string_cast.hpp>

#include <iostream>


int GraphicsEngineFrame::global_image_index = 0;

GraphicsEngineFrame::GraphicsEngineFrame(
	GraphicsEngine& engine, 
	GraphicsEngineSwapChain& parent_swapchain, 
	VkImage presentation_image) :
	GraphicsEngineBaseModule(engine),
	swap_chain(parent_swapchain),
	image_index(global_image_index++),
	analytics(60)
{
	presentation_image_view = get_graphics_engine().create_image_view(
		presentation_image, 
		swap_chain.get_image_format(), 
		VK_IMAGE_ASPECT_COLOR_BIT);
	for (Renderer* renderer : get_graphics_engine().get_renderer_mgr().get_renderers())
	{
		renderer->allocate_per_frame_resources(presentation_image, presentation_image_view);
	}

	create_synchronisation_objects();
	command_buffer = get_rsrc_mgr().create_command_buffer();

	analytics.text = std::string("Frame ") + std::to_string(image_index);
}

GraphicsEngineFrame::GraphicsEngineFrame(GraphicsEngineFrame&& frame) noexcept :
	GraphicsEngineBaseModule(frame.get_graphics_engine()),
	presentation_image_view(std::move(frame.presentation_image_view)),
	command_buffer(std::move(frame.command_buffer)),
	image_index(std::move(frame.image_index)),
	swap_chain(frame.swap_chain),
	image_available_semaphore(std::move(frame.image_available_semaphore)),
	render_finished_semaphore(std::move(frame.render_finished_semaphore)),
	fence_frame_inflight(std::move(frame.fence_frame_inflight)),
	fence_image_inflight(std::move(frame.fence_image_inflight)),
	analytics(std::move(frame.analytics)),
	objs_to_delete(std::move(frame.objs_to_delete))
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
}

void GraphicsEngineFrame::update_command_buffer()
{
	// wait until command buffer is not used anymore i.e. when frame is no longer inflight
	vkWaitForFences(get_logical_device(), 1, &fence_frame_inflight, VK_TRUE, std::numeric_limits<uint64_t>::max());
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

	//
	// do stuff that needs to be done before we record a new command buffer
	// i.e. deleting objects
	//
	pre_cmdbuffer_recording();

	auto& renderer_mgr = get_graphics_engine().get_renderer_mgr();
	if (get_graphics_engine().get_gui_manager().graphic_settings.rtx_on)
	{
		renderer_mgr.get_renderer(ERendererType::RAYTRACING).submit_draw_commands(command_buffer, presentation_image_view, image_index);
	} else
	{
		renderer_mgr.get_renderer(ERendererType::SHADOW_MAP).submit_draw_commands(command_buffer, presentation_image_view, image_index);
		renderer_mgr.get_renderer(ERendererType::RASTERIZATION).submit_draw_commands(command_buffer, presentation_image_view, image_index);
		renderer_mgr.get_renderer(ERendererType::QUAD).submit_draw_commands(command_buffer, presentation_image_view, image_index);
	}

	// Offscreen
	renderer_mgr.get_renderer(ERendererType::OFFSCREEN_GUI_VIEWPORT).submit_draw_commands(
		command_buffer, presentation_image_view, image_index);

	// ImGui
	renderer_mgr.get_renderer(ERendererType::GUI).submit_draw_commands(
		command_buffer, presentation_image_view, image_index);
	
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
	vkWaitForFences(get_logical_device(), 1, &fence_frame_inflight, VK_TRUE, std::numeric_limits<uint64_t>::max());
	analytics.stop();

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
	const auto& graphic_settings = get_graphics_engine().get_graphics_gui_manager().get_graphic_settings();
	SDS::GlobalData gubo;
	gubo.view = get_graphics_engine().get_camera()->get_view();
	gubo.proj = get_graphics_engine().get_camera()->get_projection();
	gubo.view_pos = get_graphics_engine().get_camera()->get_position();

	// light controlled by light source, only supports single light source and white lighting currently
	ObjectID entity = get_graphics_engine().get_ecs().get_global_light_source();
	auto& light_source = get_graphics_engine().get_object(entity);
	gubo.light_pos = light_source.get_game_object().get_position();
	gubo.lighting_scalar = graphic_settings.light_strength;

	get_rsrc_mgr().write_to_global_uniform_buffer(image_index, gubo);

	// TODO: this is a hacky approach, this will not work with multiple light sources
	// we will need to eventually fix this up properly
	
	const auto get_shadow_view_proj_matrix = [&](const glm::vec3& obj_pos) -> glm::mat4
	{
		// const glm::mat4 view = glm::lookAtLH(
		// 	gubo.light_pos,
		// 	obj_pos, 
		// 	Maths::forward_vec);
		const glm::mat4 view = glm::lookAtLH(
			gubo.light_pos + Maths::up_vec, 
			gubo.light_pos, 
			Maths::forward_vec);
		// const glm::vec2 horizontal_span = { -10.0f, 10.0f };
		// const glm::mat4 proj = glm::orthoLH(
		// 	horizontal_span.x, 
		// 	horizontal_span.y, 
		// 	horizontal_span.x, 
		// 	horizontal_span.y, 
		// 	0.1f, 
		// 	250.0f);

		// const auto resolution = Maths::deg2rad(45.0f); // higher is lower
		const auto resolution = Maths::deg2rad(135.0f); // higher is lower
		const glm::mat4 proj = glm::perspectiveLH(resolution, 1.0f, 0.1f, 250.0f);
		return proj * view;
	};

	// update per object uniforms
	SDS::ObjectData object_data{};
	for (const auto& [id, graphics_object] : get_graphics_engine().get_objects())
	{
		EntityFrameID efid{graphics_object->get_id(), image_index};
		object_data.model = graphics_object->get_game_object().get_transform();
		object_data.mvp = gubo.proj * gubo.view * object_data.model;
		object_data.rot_mat = glm::mat4_cast(graphics_object->get_game_object().get_rotation());
		object_data.shadow_mvp = get_shadow_view_proj_matrix(graphics_object->get_game_object().get_position()) * object_data.model;
		get_rsrc_mgr().write_to_uniform_buffer(efid, object_data);

		// TODO fix me
		// // if object contains skinned meshes update the bone matrices
		// if (graphics_object->get_render_type() == ERenderType::SKINNED)
		// {
		// 	std::vector<SDS::Bone> bones = get_graphics_engine().get_ecs().get_bones(id);
		// 	// apply object transform on the bones
		// 	for (auto& bone : bones)
		// 	{
		// 		bone.final_transform = object_data.model * bone.final_transform;
		// 	}
		// 	get_rsrc_mgr().write_to_bone_buffer(efid, bones);
		// }
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

void GraphicsEngineFrame::mark_obj_for_delete(ObjectID id)
{
	objs_to_delete.push(id);
}

void GraphicsEngineFrame::pre_cmdbuffer_recording()
{
	// Note: this code works since when we mark an object for deletion we only do so on the currently in flight frame.
	// So therefore this function only gets called once the inflight frame finishes
	// and all the frames gets cycled once. That means no frame would be using the affected resources.
	while (!objs_to_delete.empty())
	{
		get_graphics_engine().cleanup_entity(objs_to_delete.front());
		objs_to_delete.pop();
	}
}