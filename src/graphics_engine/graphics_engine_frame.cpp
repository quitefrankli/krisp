#include "graphics_engine_frame.hpp"

#include "graphics_engine.hpp"
#include "graphics_engine_swap_chain.hpp"
#include "objects/object.hpp"
#include "uniform_buffer_object.hpp"
#include "camera.hpp"

#include <glm/gtx/string_cast.hpp>

#include <iostream>


int GraphicsEngineFrame::global_image_index = 0;

GraphicsEngineFrame::GraphicsEngineFrame(GraphicsEngine& engine, GraphicsEngineSwapChain& parent_swapchain, VkImage image) :
	GraphicsEngineBaseModule(engine),
	swap_chain(parent_swapchain)
{
	this->image = image;
	image_index = global_image_index++;

	// image view
	VkImageViewCreateInfo create_info{};
	create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	create_info.image = image;
	create_info.viewType = VK_IMAGE_VIEW_TYPE_2D; // specifies how the image data should be interpreted
													// i.e. treat images as 1D, 2D, 3D textures and cube maps
	create_info.format = swap_chain.get_image_format();
	create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
	create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; // describes image purpose and which part should be accessed
	create_info.subresourceRange.baseMipLevel = 0;
	create_info.subresourceRange.levelCount = 1;
	create_info.subresourceRange.baseArrayLayer = 0;
	create_info.subresourceRange.layerCount = 1;

	if (vkCreateImageView(get_logical_device(), &create_info, nullptr, &image_view) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create image views!");
	}

	// frame buffer
	// note that while the color image is different for every frame, the depth image can be the same
	std::vector<VkImageView> attachments = { image_view, get_graphics_engine().get_depth_buffer().get_image_view() };
	VkFramebufferCreateInfo frame_buffer_create_info{};
	frame_buffer_create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	frame_buffer_create_info.renderPass = get_graphics_engine().get_render_pass();
	frame_buffer_create_info.attachmentCount = attachments.size();
	frame_buffer_create_info.pAttachments = attachments.data();
	frame_buffer_create_info.width = swap_chain.get_extent().width;
	frame_buffer_create_info.height = swap_chain.get_extent().height;
	frame_buffer_create_info.layers = 1;

	if (vkCreateFramebuffer(get_logical_device(), &frame_buffer_create_info, nullptr, &frame_buffer) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create framebuffer!");
	}

	create_synchronisation_objects();
	create_command_buffer();

	analytics.text = std::string("Frame ") + std::to_string(image_index);
}

GraphicsEngineFrame::~GraphicsEngineFrame()
{
	global_image_index--;
	vkFreeCommandBuffers(get_logical_device(), get_graphics_engine().get_command_pool(), 1, &command_buffer);

	vkDestroyImageView(get_logical_device(), image_view, nullptr);
	vkDestroyFramebuffer(get_logical_device(), frame_buffer, nullptr);

	// cleanup synchronisation objects
	vkDestroySemaphore(get_logical_device(), image_available_semaphore, nullptr);
	vkDestroySemaphore(get_logical_device(), render_finished_semaphore, nullptr);
	vkDestroyFence(get_logical_device(), fence_frame_inflight, nullptr);
	// vkDestroyFence(get_logical_device(), fence_image_inflight, nullptr); // this isn't an actual fence it's rather a reference to the inflight frame fence
}

void GraphicsEngineFrame::spawn_object(GraphicsEngineObject& object)
{
	create_descriptor_sets(object);
	// update_command_buffer();
}

void GraphicsEngineFrame::create_descriptor_sets(GraphicsEngineObject& object)
{
	std::vector<VkDescriptorSetLayout> layouts(object.get_shapes().size(), 
		get_graphics_engine().get_graphics_resource_manager().high_freq_descriptor_set_layout);

	VkDescriptorSetAllocateInfo alloc_info{};
	alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	alloc_info.descriptorPool = get_graphics_engine().get_descriptor_pool();
	alloc_info.descriptorSetCount = object.get_shapes().size();
	alloc_info.pSetLayouts = layouts.data();

	std::vector<VkDescriptorSet> new_descriptor_sets(object.get_shapes().size());

	if (vkAllocateDescriptorSets(get_logical_device(), &alloc_info, new_descriptor_sets.data()) != VK_SUCCESS)
	{
		throw std::runtime_error("GraphicsEngineFrame::spawn_object: failed to allocate descriptor sets!");
	}

	for (int vertex_set_index = 0; vertex_set_index < object.get_shapes().size(); vertex_set_index++)
	{
		std::vector<VkWriteDescriptorSet> descriptor_writes;

		VkDescriptorBufferInfo buffer_info{};
		buffer_info.buffer = object.uniform_buffer;
		buffer_info.offset = 0; // sizeof(UniformBufferObject)* (descriptor_sets.size() + vertex_set_index);
		buffer_info.range = sizeof(UniformBufferObject);

		VkWriteDescriptorSet uniform_buffer_descriptor_set{};
		uniform_buffer_descriptor_set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		uniform_buffer_descriptor_set.dstSet = new_descriptor_sets[vertex_set_index];
		uniform_buffer_descriptor_set.dstBinding = 0; // also set to 0 in the shader
		uniform_buffer_descriptor_set.dstArrayElement = 0; // offset
		uniform_buffer_descriptor_set.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		uniform_buffer_descriptor_set.descriptorCount = 1;
		uniform_buffer_descriptor_set.pBufferInfo = &buffer_info;
		uniform_buffer_descriptor_set.pImageInfo = nullptr;
		uniform_buffer_descriptor_set.pTexelBufferView = nullptr;

		descriptor_writes.push_back(uniform_buffer_descriptor_set);

		if (object.texture)
		{
			VkDescriptorImageInfo image_info{};
			image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			// some useful links when we get up to this part
			// https://gamedev.stackexchange.com/questions/146982/compressed-vs-uncompressed-textures-differences
			// https://stackoverflow.com/questions/27345340/how-do-i-render-multiple-textures-in-modern-opengl
			// for texture seams and more indepth texture atlas https://www.pluralsight.com/blog/film-games/understanding-uvs-love-them-or-hate-them-theyre-essential-to-know
			// descriptor set layout frequency https://stackoverflow.com/questions/50986091/what-is-the-best-way-of-dealing-with-textures-for-a-same-shader-in-vulkan
			image_info.imageView = object.get_texture_image_view();
			image_info.sampler = object.get_texture_sampler();

			VkWriteDescriptorSet combined_image_sampler_descriptor_set{};
			combined_image_sampler_descriptor_set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			combined_image_sampler_descriptor_set.dstSet = new_descriptor_sets[vertex_set_index];
			combined_image_sampler_descriptor_set.dstBinding = 1; // also set to 1 in the shader
			combined_image_sampler_descriptor_set.dstArrayElement = 0; // offset
			combined_image_sampler_descriptor_set.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			combined_image_sampler_descriptor_set.descriptorCount = 1;
			combined_image_sampler_descriptor_set.pBufferInfo = nullptr; 
			combined_image_sampler_descriptor_set.pImageInfo = &image_info;
			combined_image_sampler_descriptor_set.pTexelBufferView = nullptr;

			descriptor_writes.push_back(combined_image_sampler_descriptor_set);
		}

		vkUpdateDescriptorSets(get_logical_device(),
							   static_cast<uint32_t>(descriptor_writes.size()), 
							   descriptor_writes.data(), 
							   0, 
							   nullptr);
		
		object.descriptor_sets.push_back(std::move(new_descriptor_sets[vertex_set_index]));
	}
}

void GraphicsEngineFrame::create_command_buffer()
{
	VkCommandBufferAllocateInfo allocation_info{};
	allocation_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocation_info.commandPool = get_graphics_engine().get_command_pool();
	allocation_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; // specifies if allocated command buffers are primary or secondary command buffers, secondary can reuse primary
	allocation_info.commandBufferCount = 1;

	if (vkAllocateCommandBuffers(get_logical_device(), &allocation_info, &command_buffer) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to allocate command buffers!");
	}

	// update_command_buffer();
}

void GraphicsEngineFrame::update_command_buffer()
{
	// wait until command buffer is not used anymore i.e. when frame is no longer inflight
	vkWaitForFences(get_logical_device(), 1, &fence_frame_inflight, VK_TRUE, std::numeric_limits<uint64_t>::max());
	VkCommandBufferResetFlags reset_flags = 0;
	vkResetCommandBuffer(command_buffer, reset_flags);

	//
	// do stuff that needs to be done before we record a new command buffer
	// i.e. deleting objects
	//
	pre_cmdbuffer_recording();

	// starting command buffer recording
	VkCommandBufferBeginInfo begin_info{};
	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin_info.flags = 0;
	begin_info.pInheritanceInfo = nullptr;

	if (vkBeginCommandBuffer(command_buffer, &begin_info) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to begin recording command buffer!");
	}

	// starting a render pass
	VkRenderPassBeginInfo render_pass_begin_info{};
	render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	render_pass_begin_info.renderPass = get_graphics_engine().get_render_pass();
	render_pass_begin_info.framebuffer = frame_buffer;
	render_pass_begin_info.renderArea.offset = { 0, 0 };
	render_pass_begin_info.renderArea.extent = swap_chain.get_extent();
	
	std::vector<VkClearValue> clear_values(2);
	clear_values[0].color = { 0.0f, 0.0f, 0.0f, 1.0f };
	clear_values[1].depthStencil = { 1.0f, 0 };
	render_pass_begin_info.clearValueCount = clear_values.size();
	render_pass_begin_info.pClearValues = clear_values.data();
	vkCmdBeginRenderPass(command_buffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

	// vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, get_graphics_engine().get_graphics_pipeline().graphics_pipeline); // bind the graphics pipeline

	// global descriptor object, for per frame updates
	vkCmdBindDescriptorSets(command_buffer,
							VK_PIPELINE_BIND_POINT_GRAPHICS,
							get_graphics_engine().get_graphics_pipeline().pipeline_layout,
							1,
							1,
							&get_graphics_engine().get_graphics_resource_manager().global_descriptor_set,
							0,
							nullptr);

	auto per_obj_draw_fn = [&](GraphicsEngineObject& object)
	{
		const auto& shapes = object.get_shapes();

		// NOTE:A the vertex and index buffers contain the data for all the 'vertex_sets/shapes' concatenated together
		VkDeviceSize offsets[] = { 0 };
		vkCmdBindVertexBuffers(
			command_buffer, 
			0, 					// offset
			1, 					// number of bindings
			&object.vertex_buffer, 	// array of vertex buffers to bind
			offsets				// byte offset to start from for each buffer
		);
		vkCmdBindIndexBuffer(
			command_buffer,
			object.index_buffer,
			0,						// offset
			VK_INDEX_TYPE_UINT32
		);

		// this really should be per object, we will adjust in the future
		int total_vertex_offset = 0;
		int total_index_offset = 0;
		for (int vertex_set_index = 0; vertex_set_index < shapes.size(); vertex_set_index++)
		{
			const auto& shape = shapes[vertex_set_index];

			// descriptor binding, we need to bind the descriptor set for each swap chain image and for each vertex_set with different descriptor set

			vkCmdBindDescriptorSets(command_buffer, 
									VK_PIPELINE_BIND_POINT_GRAPHICS, // unlike vertex buffer, descriptor sets are not unique to the graphics pipeline, compute pipeline is also possible
									get_graphics_engine().get_graphics_pipeline().pipeline_layout, 
									0, // offset
									1, // number of sets to bind
									&object.descriptor_sets[vertex_set_index],
									0,
									nullptr);
		
			vkCmdDrawIndexed(
				command_buffer,
				shape.get_num_vertex_indices(),	// vertex count
				1,	// instance count
				total_index_offset,	// first index
				total_vertex_offset,	// first vertex index (used for offsetting and defines the lowest value of gl_VertexIndex)
				0	// first instance, used as offset for instance rendering, defines the lower value of gl_InstanceIndex
			);

			// this is not get_num_vertex_indices() because we want to offset the vertex set essentially
			// see NOTE:A
			total_vertex_offset += shape.get_num_unique_vertices();
			total_index_offset += shape.get_num_vertex_indices();
		}
	};

	using type_t = GraphicsEnginePipeline::PIPELINE_TYPE;
	for (const auto& it_pair : get_graphics_engine().get_objects())
	{
		const auto& graphics_object = it_pair.second;
		if (graphics_object->is_marked_for_delete())
			continue;

		if (!graphics_object->get_game_object().get_visibility())
			continue;
			
		auto pipeline_type = graphics_object->texture ? type_t::STANDARD : type_t::COLOR;
		if (get_graphics_engine().is_wireframe_mode)
		{
			pipeline_type = type_t::WIREFRAME;
		}
		vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, get_graphics_engine().get_pipeline(pipeline_type)); // bind the graphics pipeline

		per_obj_draw_fn(*graphics_object);
	}

	// render gui
	get_graphics_engine().get_gui_manager().add_render_cmd(command_buffer);

	vkCmdEndRenderPass(command_buffer);
	
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

	VkPresentInfoKHR present_info{};
	present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	present_info.waitSemaphoreCount = 1;
	// the semaphore to wait on before presentation
	present_info.pWaitSemaphores = signal_semaphores;

	VkSwapchainKHR swap_chains[] = { swap_chain.get_swap_chain() };
	present_info.swapchainCount = 1;
	present_info.pSwapchains = swap_chains;
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
	void* data;

	// update global uniform buffer
	auto& graphic_settings = get_graphics_engine().get_gui_manager().graphic_settings;
	GlobalUniformBufferObject gubo;
	gubo.view = get_graphics_engine().get_camera()->get_view(); // we can move this to push constant
	gubo.proj = get_graphics_engine().get_camera()->get_perspective(); // we can move this to push constant
	gubo.view_pos = get_graphics_engine().get_camera()->get_position();
	gubo.light_pos = graphic_settings.light_ray.origin;
	gubo.lighting = graphic_settings.light_strength;
	vkMapMemory(get_logical_device(), get_graphics_engine().get_global_uniform_buffer_memory(), 0, sizeof(gubo), 0, &data);
	memcpy(data, &gubo, sizeof(gubo));
	vkUnmapMemory(get_logical_device(), get_graphics_engine().get_global_uniform_buffer_memory());

	// update per object uniforms
	UniformBufferObject default_ubo{};
	default_ubo.model = glm::mat4(1);
	for (const auto& it_pair : get_graphics_engine().get_objects())
	{
		const auto& graphics_object = it_pair.second;
		default_ubo.model = graphics_object->get_game_object().get_transform();
		default_ubo.mvp = gubo.proj * gubo.view * default_ubo.model;
		default_ubo.rot_mat = glm::mat4_cast(graphics_object->get_game_object().get_rotation());
		vkMapMemory(get_logical_device(), graphics_object->uniform_buffer_memory, 0, sizeof(UniformBufferObject), 0, &data);
		memcpy(data, &default_ubo, sizeof(UniformBufferObject));
		vkUnmapMemory(get_logical_device(), graphics_object->uniform_buffer_memory);
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

void GraphicsEngineFrame::mark_obj_for_delete(uint64_t id)
{
	objs_to_delete.push(id);
}

void GraphicsEngineFrame::pre_cmdbuffer_recording()
{
	while (!objs_to_delete.empty())
	{
		auto id = objs_to_delete.front();
		get_graphics_engine().get_objects().erase(id);
		get_graphics_engine().get_objects()[id]->descriptor_sets[0];
		objs_to_delete.pop();
	}
}