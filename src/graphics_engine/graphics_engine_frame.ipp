#pragma once

#include "graphics_engine_frame.hpp"

#include "graphics_engine.hpp"
#include "graphics_engine_swap_chain.hpp"
#include "objects/object.hpp"
#include "objects/light_source.hpp"
#include "shared_data_structures.hpp"
#include "camera.hpp"
#include "pipeline/pipeline_types.hpp"

#include <glm/gtx/string_cast.hpp>

#include <iostream>


template<typename GraphicsEngineT>
int GraphicsEngineFrame<GraphicsEngineT>::global_image_index = 0;

template<typename GraphicsEngineT>
GraphicsEngineFrame<GraphicsEngineT>::GraphicsEngineFrame(
	GraphicsEngineT& engine, 
	GraphicsEngineSwapChain<GraphicsEngineT>& parent_swapchain, 
	VkImage presentation_image) :
	GraphicsEngineBaseModule<GraphicsEngineT>(engine),
	swap_chain(parent_swapchain),
	image_index(global_image_index++)
{
	presentation_image_view = get_graphics_engine().create_image_view(
		presentation_image, 
		swap_chain.get_image_format(), 
		VK_IMAGE_ASPECT_COLOR_BIT);
	for (Renderer<GraphicsEngineT>* renderer : get_graphics_engine().get_renderer_mgr().get_renderers())
	{
		renderer->allocate_per_frame_resources(presentation_image, presentation_image_view);
	}

	create_synchronisation_objects();
	command_buffer = get_rsrc_mgr().create_command_buffer();

	analytics.text = std::string("Frame ") + std::to_string(image_index);
}

template<typename GraphicsEngineT>
GraphicsEngineFrame<GraphicsEngineT>::GraphicsEngineFrame(GraphicsEngineFrame&& frame) noexcept :
	GraphicsEngineBaseModule<GraphicsEngineT>(frame.get_graphics_engine()),
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

template<typename GraphicsEngineT>
GraphicsEngineFrame<GraphicsEngineT>::~GraphicsEngineFrame()
{
	if (!should_destroy)
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

template<typename GraphicsEngineT>
void GraphicsEngineFrame<GraphicsEngineT>::spawn_object(GraphicsEngineObject<GraphicsEngineT>& object)
{
	create_descriptor_sets(object);
	// update_command_buffer();
}

template<typename GraphicsEngineT>
void GraphicsEngineFrame<GraphicsEngineT>::create_descriptor_sets(GraphicsEngineObject<GraphicsEngineT>& object)
{
	auto& engine = get_graphics_engine();
	std::vector<VkDescriptorSet> new_descriptor_sets = engine.get_rsrc_mgr().reserve_high_frequency_dsets(object.get_shapes().size());
	assert(new_descriptor_sets.size() == object.get_shapes().size());

	for (int vertex_set_index = 0; vertex_set_index < object.get_shapes().size(); vertex_set_index++)
	{
		std::vector<VkWriteDescriptorSet> descriptor_writes;

		VkDescriptorBufferInfo buffer_info{};
		const GraphicsBuffer::Slot buffer_slot = get_rsrc_mgr().get_uniform_buffer_slot(object.get_game_object().get_id());
		buffer_info.buffer = get_rsrc_mgr().get_uniform_buffer();
		buffer_info.offset = buffer_slot.offset;
		buffer_info.range = buffer_slot.size;

		VkWriteDescriptorSet uniform_buffer_descriptor_set{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
		uniform_buffer_descriptor_set.dstSet = new_descriptor_sets[vertex_set_index];
		uniform_buffer_descriptor_set.dstBinding = 0; // also set to 0 in the shader
		uniform_buffer_descriptor_set.dstArrayElement = 0; // offset
		uniform_buffer_descriptor_set.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		uniform_buffer_descriptor_set.descriptorCount = 1;
		uniform_buffer_descriptor_set.pBufferInfo = &buffer_info;

		descriptor_writes.push_back(uniform_buffer_descriptor_set);

		switch (object.get_render_type())
		{
			case EPipelineType::STANDARD:
			case EPipelineType::CUBEMAP:
				{
					VkDescriptorImageInfo image_info{};
					image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
					// some useful links when we get up to this part
					// https://gamedev.stackexchange.com/questions/146982/compressed-vs-uncompressed-textures-differences
					// https://stackoverflow.com/questions/27345340/how-do-i-render-multiple-textures-in-modern-opengl
					// for texture seams and more indepth texture atlas https://www.pluralsight.com/blog/film-games/understanding-uvs-love-them-or-hate-them-theyre-essential-to-know
					// descriptor set layout frequency https://stackoverflow.com/questions/50986091/what-is-the-best-way-of-dealing-with-textures-for-a-same-shader-in-vulkan
					image_info.imageView = object.get_materials()[vertex_set_index].get_texture().get_texture_image_view();
					image_info.sampler = object.get_materials()[vertex_set_index].get_texture().get_texture_sampler();

					VkWriteDescriptorSet combined_image_sampler_descriptor_set{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
					combined_image_sampler_descriptor_set.dstSet = new_descriptor_sets[vertex_set_index];
					combined_image_sampler_descriptor_set.dstBinding = 1; // also set to 1 in the shader
					combined_image_sampler_descriptor_set.dstArrayElement = 0; // offset
					combined_image_sampler_descriptor_set.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
					combined_image_sampler_descriptor_set.descriptorCount = 1;
					combined_image_sampler_descriptor_set.pImageInfo = &image_info;

					descriptor_writes.push_back(combined_image_sampler_descriptor_set);
				}
				break;
			default:
				break;
		}

		const GraphicsBuffer::Slot mat_slot = 
			get_rsrc_mgr().get_materials_buffer_slot(object.get_shapes()[vertex_set_index].get_id());
		VkDescriptorBufferInfo material_buffer_info{};
		material_buffer_info.buffer = get_rsrc_mgr().get_materials_buffer();
		material_buffer_info.offset = mat_slot.offset;
		material_buffer_info.range = mat_slot.size;
		VkWriteDescriptorSet material_buffer_dset{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
		material_buffer_dset.dstSet = new_descriptor_sets[vertex_set_index];
		material_buffer_dset.dstBinding = 2;
		material_buffer_dset.dstArrayElement = 0;
		material_buffer_dset.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		material_buffer_dset.descriptorCount = 1;
		material_buffer_dset.pBufferInfo = &material_buffer_info;

		descriptor_writes.push_back(material_buffer_dset);

		vkUpdateDescriptorSets(get_logical_device(),
							   static_cast<uint32_t>(descriptor_writes.size()), 
							   descriptor_writes.data(), 
							   0, 
							   nullptr);
		
		object.descriptor_sets.push_back(std::move(new_descriptor_sets[vertex_set_index]));
	}
}

template<typename GraphicsEngineT>
void GraphicsEngineFrame<GraphicsEngineT>::update_command_buffer()
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
	Renderer<GraphicsEngineT>& main_renderer = renderer_mgr.get_renderer(
		get_graphics_engine().get_gui_manager().graphic_settings.rtx_on() ?
		ERendererType::RAYTRACING : ERendererType::RASTERIZATION);
	Renderer<GraphicsEngineT>& gui_renderer = renderer_mgr.get_renderer(ERendererType::GUI);

	main_renderer.submit_draw_commands(command_buffer, presentation_image_view, image_index);
	gui_renderer.submit_draw_commands(command_buffer, presentation_image_view, image_index);
	
	if (vkEndCommandBuffer(command_buffer) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to record command buffer!");
	}
}

template<typename GraphicsEngineT>
void GraphicsEngineFrame<GraphicsEngineT>::draw()
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

template<typename GraphicsEngineT>
void GraphicsEngineFrame<GraphicsEngineT>::update_uniform_buffer()
{
	// update global uniform buffer
	auto& graphic_settings = get_graphics_engine().get_graphics_gui_manager().graphic_settings;
	SDS::GlobalData gubo;
	gubo.view = get_graphics_engine().get_camera()->get_view(); // we can move this to push constant
	gubo.proj = get_graphics_engine().get_camera()->get_projection(); // we can move this to push constant
	gubo.view_pos = get_graphics_engine().get_camera()->get_position();

	// light controlled by light source, only supports single light source and white lighting currently
	auto& light_sources = get_graphics_engine().get_light_sources();
	if (light_sources.empty())
	{
		gubo.light_pos = {};
	} else {
		gubo.light_pos = light_sources.begin()->second.get().get_position();
	}
	gubo.lighting_scalar = graphic_settings.light_strength;

	get_rsrc_mgr().write_to_global_uniform_buffer(gubo);

	// update per object uniforms
	SDS::ObjectData object_data{};
	for (const auto& it_pair : get_graphics_engine().get_objects())
	{
		const auto& graphics_object = it_pair.second;
		object_data.model = graphics_object->get_game_object().get_transform();
		object_data.mvp = gubo.proj * gubo.view * object_data.model;
		object_data.rot_mat = glm::mat4_cast(graphics_object->get_game_object().get_rotation());

		get_rsrc_mgr().write_to_uniform_buffer(object_data, graphics_object->get_game_object().get_id());
	}
}

template<typename GraphicsEngineT>
void GraphicsEngineFrame<GraphicsEngineT>::create_synchronisation_objects()
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

template<typename GraphicsEngineT>
void GraphicsEngineFrame<GraphicsEngineT>::mark_obj_for_delete(uint64_t id)
{
	objs_to_delete.push(id);
}

template<typename GraphicsEngineT>
void GraphicsEngineFrame<GraphicsEngineT>::pre_cmdbuffer_recording()
{
	while (!objs_to_delete.empty())
	{
		const auto id = objs_to_delete.front();
		get_rsrc_mgr().free_vertex_buffer(id);
		get_rsrc_mgr().free_index_buffer(id);
		get_rsrc_mgr().free_uniform_buffer(id);
		for (const auto& shape : get_graphics_engine().get_object(id).get_shapes())
		{
			get_rsrc_mgr().free_materials_buffer(shape.get_id());
		}
		get_graphics_engine().get_objects().erase(id);
		get_graphics_engine().get_light_sources().erase(id);
		objs_to_delete.pop();
	}
}