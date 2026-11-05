#include "renderers.hpp"

#include "graphics_engine/graphics_engine.hpp"
#include "graphics_engine/pipeline/pipeline.hpp"
#include "graphics_engine/dynamic_rendering.hpp"


PresentationRenderer::PresentationRenderer(GraphicsEngine& engine) :
	Renderer(engine)
{
	// The resolve image matches the swap-chain extent, so filtering is only
	// relevant at edge/sample-coordinate precision and no mip chain is needed.
	VkSamplerCreateInfo sampler_info{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
	sampler_info.magFilter = VK_FILTER_LINEAR;
	sampler_info.minFilter = VK_FILTER_LINEAR;
	sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
	sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sampler_info.maxLod = 0.0f;
	if (vkCreateSampler(get_logical_device(), &sampler_info, nullptr, &scene_sampler) != VK_SUCCESS)
		throw std::runtime_error("failed to create HDR presentation sampler");
}

PresentationRenderer::~PresentationRenderer()
{
	get_rsrc_mgr().free_dsets(scene_inputs);
	vkDestroySampler(get_logical_device(), scene_sampler, nullptr);
}

void PresentationRenderer::allocate_per_frame_resources(VkImage presentation_image, VkImageView presentation_image_view)
{
	presentation_images.push_back(presentation_image);
	presentation_image_views.push_back(presentation_image_view);

	VkDescriptorSetLayoutBinding binding{};
	binding.binding = 0;
	binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	binding.descriptorCount = 1;
	binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	const VkDescriptorSetLayout layout = get_rsrc_mgr().request_dset_layout({binding});
	const VkDescriptorSet input = get_rsrc_mgr().reserve_dset(layout);
	scene_inputs.push_back(input);

	const auto frame_index = static_cast<uint32_t>(scene_inputs.size() - 1);
	// GraphicsEngineFrame allocates renderers in renderer-type order, with the
	// raster resolve for this index already available when presentation is wired.
	const VkImageView scene_view = get_graphics_engine().get_renderer_mgr()
		.get_renderer(ERendererType::RASTERIZATION).get_output_image_view(frame_index);
	VkDescriptorImageInfo image_info{};
	image_info.sampler = scene_sampler;
	image_info.imageView = scene_view;
	image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
	write.dstSet = input;
	write.dstBinding = 0;
	write.descriptorCount = 1;
	write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	write.pImageInfo = &image_info;
	vkUpdateDescriptorSets(get_logical_device(), 1, &write, 0, nullptr);
}

void PresentationRenderer::submit_draw_commands(
	VkCommandBuffer command_buffer, VkImageView, const uint32_t frame_index)
{
	const VkImage image = presentation_images.at(frame_index);
	const VkImageView image_view = presentation_image_views.at(frame_index);
	command_image_barrier(command_buffer, image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
	VkRenderingAttachmentInfo color_info{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
	color_info.imageView = image_view;
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

	auto* pipeline = get_graphics_engine().get_pipeline_mgr().fetch_pipeline(
		{ERenderType::PRESENTATION, EPipelineModifier::NONE});
	// Exposure is immutable frame state. The shader applies exp2(EV), then the
	// documented ACES-inspired curve, before the attachment's sRGB encoding.
	const SDS::PresentationPushConstant push_constant{
		.exposure_ev = get_graphics_engine().get_render_frame().view.exposure_ev,
	};
	vkCmdPushConstants(command_buffer, pipeline->pipeline_layout,
		VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push_constant), &push_constant);
	vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
		pipeline->pipeline_layout, 0, 1, &scene_inputs.at(frame_index), 0, nullptr);
	vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->graphics_pipeline);
	vkCmdDraw(command_buffer, 3, 1, 0, 0);
	vkCmdEndRendering(command_buffer);
	command_image_barrier(command_buffer, image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0);
}
