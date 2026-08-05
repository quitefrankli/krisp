#include "renderers.hpp"

#include "graphics_engine/graphics_engine.hpp"
#include "graphics_engine/pipeline/pipeline.hpp"


PresentationRenderer::PresentationRenderer(GraphicsEngine& engine) :
	Renderer(engine)
{
	create_render_pass();

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

void PresentationRenderer::allocate_per_frame_resources(VkImage, VkImageView presentation_image_view)
{
	// This pass owns no output image: each framebuffer targets its swap-chain
	// image directly after reading the corresponding raster resolve image.
	VkFramebufferCreateInfo framebuffer_info{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
	framebuffer_info.renderPass = render_pass;
	framebuffer_info.attachmentCount = 1;
	framebuffer_info.pAttachments = &presentation_image_view;
	const auto extent = get_extent();
	framebuffer_info.width = extent.width;
	framebuffer_info.height = extent.height;
	framebuffer_info.layers = 1;

	VkFramebuffer framebuffer = VK_NULL_HANDLE;
	if (vkCreateFramebuffer(get_logical_device(), &framebuffer_info, nullptr, &framebuffer) != VK_SUCCESS)
		throw std::runtime_error("failed to create HDR presentation framebuffer");
	frame_buffers.push_back(framebuffer);

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
	VkRenderPassBeginInfo begin_info{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
	begin_info.renderPass = render_pass;
	begin_info.framebuffer = frame_buffers.at(frame_index);
	begin_info.renderArea.extent = get_extent();
	VkClearValue clear{};
	clear.color = {{0.0f, 0.0f, 0.0f, 1.0f}};
	begin_info.clearValueCount = 1;
	begin_info.pClearValues = &clear;
	vkCmdBeginRenderPass(command_buffer, &begin_info, VK_SUBPASS_CONTENTS_INLINE);

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
	vkCmdEndRenderPass(command_buffer);
}

void PresentationRenderer::create_render_pass()
{
	VkAttachmentDescription color{};
	color.format = VK_FORMAT_B8G8R8A8_SRGB;
	color.samples = VK_SAMPLE_COUNT_1_BIT;
	color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	// Presentation fully overwrites the swap-chain image. The following GUI pass
	// loads it from PRESENT_SRC_KHR and returns it to the same layout.
	color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	color.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	VkAttachmentReference color_ref{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &color_ref;
	VkSubpassDependency dependency{};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	// Make the raster pass's resolve writes visible to fragment sampling. Both
	// passes are recorded into the same command buffer in this order.
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependency.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	dependency.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

	VkRenderPassCreateInfo info{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
	info.attachmentCount = 1;
	info.pAttachments = &color;
	info.subpassCount = 1;
	info.pSubpasses = &subpass;
	info.dependencyCount = 1;
	info.pDependencies = &dependency;
	if (vkCreateRenderPass(get_logical_device(), &info, nullptr, &render_pass) != VK_SUCCESS)
		throw std::runtime_error("failed to create HDR presentation render pass");
}
