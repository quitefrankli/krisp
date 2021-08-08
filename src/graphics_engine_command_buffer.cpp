#include "graphics_engine.hpp"

void GraphicsEngine::create_command_pool()
{
	QueueFamilyIndices queue_family_indices = findQueueFamilies(physicalDevice);
	VkCommandPoolCreateInfo command_pool_create_info{};
	command_pool_create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	command_pool_create_info.queueFamilyIndex = queue_family_indices.graphicsFamily.value();
	command_pool_create_info.flags = 0;

	if (vkCreateCommandPool(logical_device, &command_pool_create_info, nullptr, &command_pool) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create command pool!");
	}
}

void GraphicsEngine::create_command_buffers()
{
	command_buffers.resize(swap_chain_frame_buffers.size());
	std::cout<<"cmd buffer size " <<command_buffers.size()<<std::endl;
	VkCommandBufferAllocateInfo allocation_info{};
	allocation_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocation_info.commandPool = command_pool;
	allocation_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; // specifies if allocated command buffers are primary or secondary command buffers, secondary can reuse primary
	allocation_info.commandBufferCount = (uint32_t)command_buffers.size();

	if (vkAllocateCommandBuffers(logical_device, &allocation_info, command_buffers.data()) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to allocate command buffers!");
	}

	// starting command buffer recording
	for (int i = 0; i < command_buffers.size(); i++)
	{
		VkCommandBufferBeginInfo begin_info{};
		begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		begin_info.flags = 0;
		begin_info.pInheritanceInfo = nullptr;

		if (vkBeginCommandBuffer(command_buffers[i], &begin_info) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to begin recording command buffer!");
		}

		// starting a render pass
		VkRenderPassBeginInfo render_pass_begin_info{};
		render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		render_pass_begin_info.renderPass = render_pass;
		render_pass_begin_info.framebuffer = swap_chain_frame_buffers[i];
		render_pass_begin_info.renderArea.offset = { 0, 0 };
		render_pass_begin_info.renderArea.extent = swap_chain_extent;
		VkClearValue clear_color = { 0.0f, 0.0f, 0.0f, 1.0f };
		render_pass_begin_info.clearValueCount = 1;
		render_pass_begin_info.pClearValues = &clear_color;
		vkCmdBeginRenderPass(command_buffers[i], &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

		vkCmdBindPipeline(command_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_engine_pipeline); // bind the graphics pipeline

		//
		// binding the vertex buffer
		//

		VkBuffer vertex_buffers[] = { vertex_buffer };
		VkDeviceSize offsets[] = { 0 };
		vkCmdBindVertexBuffers(
			command_buffers[i], 
			0, 					// offset
			1, 					// number of bindings
			vertex_buffers, 	// array of vertex buffers to bind
			offsets				// byte offset to start from for each buffer
		);

		// descriptor binding, ew need to bind the descriptor set for each swap chain image
		vkCmdBindDescriptorSets(command_buffers[i], 
								VK_PIPELINE_BIND_POINT_GRAPHICS, // unlike vertex buffer, descriptor sets are not unique to the graphics pipeline, compute pipeline is also possible
								pipeline_layout, 
								0, // offset
								1, // number of sets to bind
								&descriptor_sets[i],
								0,
								nullptr);

		vkCmdDraw(
			command_buffers[i], 
			static_cast<uint32_t>(vertices.size()), // vertex count
			1, // instance count (only used for instance rendering)
			0, // first vertex index (used for offsetting and defines the lowest value of gl_VertexIndex)
			0  // first instance, used as offset for instance rendering, defines the lower value of gl_InstanceIndex
		);

		vkCmdEndRenderPass(command_buffers[i]);
		if (vkEndCommandBuffer(command_buffers[i]) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to record command buffer!");
		}
	}
}