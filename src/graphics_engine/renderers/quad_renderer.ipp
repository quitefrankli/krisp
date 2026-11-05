#include "renderers.hpp"
#include "graphics_engine/dynamic_rendering.hpp"


QuadRenderer::QuadRenderer(GraphicsEngine& engine) :
	Renderer(engine)
{
	VkDescriptorSetLayoutBinding binding{};
	binding.binding = 0;
	binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	binding.descriptorCount = 1;
	binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	binding.pImmutableSamplers = nullptr; // Optional

	texture_layout = this->get_rsrc_mgr().request_dset_layout({binding});
}

QuadRenderer::~QuadRenderer()
{
	this->get_rsrc_mgr().free_dsets(textures);
	for (auto& attachment : color_attachments)
	{
		attachment.destroy(get_logical_device());
	}
}

void QuadRenderer::allocate_per_frame_resources(VkImage, VkImageView)
{
	textures.push_back(get_rsrc_mgr().reserve_dset(texture_layout));
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
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		color_attachment.image,
		color_attachment.image_memory,
		get_msaa_sample_count());
	color_attachment.image_view = get_graphics_engine().create_image_view(
		color_attachment.image, 
		get_image_format(),
		VK_IMAGE_ASPECT_COLOR_BIT);

	color_attachments.push_back(color_attachment);
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

	const auto view = renderer_source
		? get_graphics_engine().get_renderer_mgr().get_renderer(*renderer_source)
			.get_output_image_view(frame_index)
		: texture_view;
	// This frame's fence has completed; update only its own descriptor.
	VkDescriptorImageInfo image_info{texture_sampler, view,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
	VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
	write.dstSet = textures.at(frame_index);
	write.dstBinding = 0;
	write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	write.descriptorCount = 1;
	write.pImageInfo = &image_info;
	vkUpdateDescriptorSets(get_logical_device(), 1, &write, 0, nullptr);

	auto& attachment = color_attachments.at(frame_index);
	command_image_barrier(command_buffer, attachment.image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
	VkRenderingAttachmentInfo color_info{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
	color_info.imageView = attachment.image_view;
	color_info.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	color_info.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	color_info.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	color_info.clearValue.color = {{0.0f, 0.0f, 0.0f, 1.0f}};
	VkRenderingInfo rendering_info{VK_STRUCTURE_TYPE_RENDERING_INFO};
	rendering_info.renderArea = {{0, 0}, get_extent()};
	rendering_info.layerCount = 1;
	rendering_info.colorAttachmentCount = 1;
	rendering_info.pColorAttachments = &color_info;
	vkCmdBeginRendering(command_buffer, &rendering_info);

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
		&textures.at(frame_index),
		0,
		nullptr);

	vkCmdBindPipeline(
		command_buffer, 
		VK_PIPELINE_BIND_POINT_GRAPHICS, 
		get_graphics_engine().get_pipeline_mgr().fetch_pipeline(
			{ ERenderType::QUAD, EPipelineModifier::NONE })->graphics_pipeline);
			
	vkCmdDraw(command_buffer, 3, 1, 0, 0);

	vkCmdEndRendering(command_buffer);
	command_image_barrier(command_buffer, attachment.image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT);
}

void QuadRenderer::set_texture(VkImageView view, VkSampler sampler)
{
	renderer_source.reset();
	texture_view = view;
	texture_sampler = sampler;
	should_render = view && sampler;
}

void QuadRenderer::set_renderer_source(const ERendererType source, const VkSampler sampler)
{
	renderer_source = source;
	texture_sampler = sampler;
	should_render = source != ERendererType::NONE && sampler;
}
