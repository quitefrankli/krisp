#include "renderers.hpp"


QuadRenderer::QuadRenderer(GraphicsEngine& engine) :
	Renderer(engine)
{
	create_render_pass();

	VkDescriptorSetLayoutBinding binding{};
	binding.binding = 0;
	binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	binding.descriptorCount = 1;
	binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	binding.pImmutableSamplers = nullptr; // Optional

	VkDescriptorSetLayout layout = this->get_rsrc_mgr().request_dset_layout({binding});

	texture = this->get_rsrc_mgr().reserve_dset(layout);
}

QuadRenderer::~QuadRenderer()
{
	this->get_rsrc_mgr().free_dset(texture);
	for (auto& attachment : color_attachments)
	{
		attachment.destroy(get_logical_device());
	}
}

void QuadRenderer::allocate_per_frame_resources(VkImage, VkImageView)
{	
	//
	// Generate attachments
	//
	RenderingAttachment color_attachment;
	const auto extent = get_extent();
	get_graphics_engine().create_image(
		extent.width, 
		extent.height,
		get_image_format(),
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		color_attachment.image,
		color_attachment.image_memory,
		get_msaa_sample_count());
	color_attachment.image_view = get_graphics_engine().create_image_view(
		color_attachment.image, 
		get_image_format(),
		VK_IMAGE_ASPECT_COLOR_BIT);

	color_attachments.push_back(color_attachment);

	//
	// Create framebuffer
	//
	std::vector<VkImageView> attachments { 
		color_attachment.image_view // main attachment color image_view, that shaders write to
	};

	VkFramebufferCreateInfo frame_buffer_create_info{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
	frame_buffer_create_info.renderPass = this->render_pass;
	frame_buffer_create_info.attachmentCount = attachments.size();
	frame_buffer_create_info.pAttachments = attachments.data();
	frame_buffer_create_info.width = extent.width;
	frame_buffer_create_info.height = extent.height;
	frame_buffer_create_info.layers = 1;

	VkFramebuffer new_frame_buffer;
	if (vkCreateFramebuffer(get_logical_device(), &frame_buffer_create_info, nullptr, &new_frame_buffer) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create framebuffer!");
	}
	this->frame_buffers.push_back(new_frame_buffer);
}

void QuadRenderer::submit_draw_commands(
	VkCommandBuffer command_buffer,
	VkImageView,
	uint32_t frame_index)
{
	if (!should_render)
	{
		return;
	}

	if (texture_to_render)
	{
		if (texture_to_render.value().render_frames_remaining-- > 0)
		{
			return;
		}

		VkDescriptorImageInfo image_info{};
		image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		image_info.imageView = texture_to_render.value().texture_view;
		image_info.sampler = texture_to_render.value().texture_sampler;

		VkWriteDescriptorSet combined_image_sampler_descriptor_set{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
		combined_image_sampler_descriptor_set.dstSet = texture;
		combined_image_sampler_descriptor_set.dstBinding = 0;
		combined_image_sampler_descriptor_set.dstArrayElement = 0; // offset
		combined_image_sampler_descriptor_set.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		combined_image_sampler_descriptor_set.descriptorCount = 1;
		combined_image_sampler_descriptor_set.pImageInfo = &image_info;

		vkUpdateDescriptorSets(
			get_logical_device(),
			1,
			&combined_image_sampler_descriptor_set,
			0,
			nullptr);

		texture_to_render = std::nullopt;
	}

	// starting a render pass
	VkRenderPassBeginInfo render_pass_begin_info{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
	render_pass_begin_info.renderPass = this->render_pass;
	render_pass_begin_info.framebuffer = this->frame_buffers[frame_index];
	render_pass_begin_info.renderArea.offset = { 0, 0 };
	render_pass_begin_info.renderArea.extent = get_extent();
	
	VkClearValue clear_value;
	clear_value.color = { 0.0f, 0.0f, 0.0f, 1.0f };
	render_pass_begin_info.clearValueCount = 1;
	render_pass_begin_info.pClearValues = &clear_value;
	vkCmdBeginRenderPass(command_buffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

	SDS::QuadRendererPushConstant push_constant{};
	push_constant.flags = sampling_flags;

	vkCmdPushConstants(
		command_buffer,
		get_graphics_engine().get_pipeline_mgr().fetch_pipeline(
			{ ERenderType::QUAD, EPipelineModifier::NONE })->pipeline_layout,
		VK_SHADER_STAGE_FRAGMENT_BIT,
		0,
		sizeof(push_constant),
		&push_constant);

	vkCmdBindDescriptorSets(
		command_buffer,
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		get_graphics_engine().get_pipeline_mgr().fetch_pipeline(
			{ ERenderType::QUAD, EPipelineModifier::NONE })->pipeline_layout,
		0,
		1,
		&texture,
		0,
		nullptr);

	vkCmdBindPipeline(
		command_buffer, 
		VK_PIPELINE_BIND_POINT_GRAPHICS, 
		get_graphics_engine().get_pipeline_mgr().fetch_pipeline(
			{ ERenderType::QUAD, EPipelineModifier::NONE })->graphics_pipeline);
			
	vkCmdDraw(command_buffer, 3, 1, 0, 0);

	vkCmdEndRenderPass(command_buffer);
}

void QuadRenderer::set_texture(VkImageView texture_view, VkSampler texture_sampler)
{
	should_render = texture_view && texture_sampler;
	if (!should_render)
	{
		return;
	}

	texture_to_render->texture_view = texture_view;
	texture_to_render->texture_sampler = texture_sampler;
	// this avoids the issue of updating a dset while it's being used
	texture_to_render->render_frames_remaining = get_graphics_engine().get_num_swapchain_images();
}

void QuadRenderer::create_render_pass()
{
	//
	// Color Attachment
	//
	VkAttachmentDescription color_attachment{}; // main attachment that our shaders write to
	color_attachment.format = get_image_format();
	color_attachment.samples = this->get_msaa_sample_count(); // >1 if we are doing multisampling	
	color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; // determine what to do with the data in the attachment before rendering
	color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE; // dtermine what to do with the data in the attachment after rendering
	color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; // specifies which layout the image will have before the render pass begins
	color_attachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; // specifies the layout to automatically transition to when the render pass finishes

	// subpasses and attachment references
	// a single render pass can consist of multiple subpasses
	VkAttachmentReference color_attachment_ref{};
	color_attachment_ref.attachment = 0; // only works since we only have 1 attachment description
	color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	//
	// Subpass
	//
	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS; // as opposed to compute subpass
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &color_attachment_ref;

	// render pass
	VkSubpassDependency dependency{};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	// depth image is accessed early in the frament test pipeline stage
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dependency.srcAccessMask = 0;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

	std::vector<VkAttachmentDescription> attachments{ color_attachment };
	VkRenderPassCreateInfo render_pass_create_info{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
	render_pass_create_info.attachmentCount = attachments.size();
	render_pass_create_info.pAttachments = attachments.data();
	render_pass_create_info.subpassCount = 1;
	render_pass_create_info.pSubpasses = &subpass;
	render_pass_create_info.dependencyCount = 1;
	render_pass_create_info.pDependencies = &dependency;
	
	if (vkCreateRenderPass(get_logical_device(), &render_pass_create_info, nullptr, &this->render_pass) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create render pass!");
	}
}
