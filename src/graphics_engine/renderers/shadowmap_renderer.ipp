#include "renderers.hpp"
#include <algorithm>
#include <bit>
#include "shared_data_structures.hpp"


static constexpr uint32_t SHADOW_MAP_LAYER_COUNT = 6; // 6 sided cube

ShadowMapRenderer::ShadowMapRenderer(GraphicsEngine& engine) :
	Renderer(engine)
{
	create_render_pass();
	create_sampler();
}

ShadowMapRenderer::~ShadowMapRenderer()
{
	for (auto& frame : frames)
	{
		get_rsrc_mgr().free_dset(frame.descriptor);
		destroy_frame_images(frame);
	}
	vkDestroySampler(get_logical_device(), shadow_map_sampler, nullptr);
}

void ShadowMapRenderer::destroy_frame_images(FrameResources& frame)
{
	vkDestroyImageView(get_logical_device(), frame.preview_view, nullptr);
	vkDestroyImageView(get_logical_device(), frame.cube_array_view, nullptr);
	frame.attachment.destroy(get_logical_device());
	frame.preview_view = VK_NULL_HANDLE;
	frame.cube_array_view = VK_NULL_HANDLE;
	frame.attachment = {};
}

void ShadowMapRenderer::allocate_per_frame_resources(VkImage, VkImageView)
{
	FrameResources frame;
	frame.descriptor = get_rsrc_mgr().reserve_dset(get_rsrc_mgr().get_shadow_map_dset_layout());
	frames.push_back(frame);
	frame_buffers.push_back(VK_NULL_HANDLE);
	// A tiny cube array remains bindable when the scene has no shadowed lights.
	grow_frame_resources(static_cast<uint32_t>(frames.size() - 1), 0);
}

void ShadowMapRenderer::prepare_frame(const uint32_t frame_idx, PointLightingData lighting)
{
	auto& frame = frames.at(frame_idx);
	if (lighting.shadow_count > frame.capacity)
		grow_frame_resources(frame_idx, std::bit_ceil(lighting.shadow_count));
	frame.lighting = std::move(lighting);
}

void ShadowMapRenderer::grow_frame_resources(const uint32_t frame_idx, const uint32_t capacity)
{
	const uint32_t layer_count = SHADOW_MAP_LAYER_COUNT * std::max(capacity, 1u);
	const auto extent = capacity ? get_extent() : VkExtent2D{1, 1};
	FrameResources replacement;
	VkFramebuffer framebuffer = VK_NULL_HANDLE;
	try
	{
		get_graphics_engine().create_image(
			extent.width, extent.height, get_image_format(), VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			replacement.attachment.image, replacement.attachment.image_memory,
			get_msaa_sample_count(), layer_count, VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT);
		replacement.attachment.image_view = get_graphics_engine().create_image_view(
			replacement.attachment.image, get_image_format(), VK_IMAGE_ASPECT_DEPTH_BIT,
			VK_IMAGE_VIEW_TYPE_2D_ARRAY, layer_count);
		replacement.cube_array_view = get_graphics_engine().create_image_view(
			replacement.attachment.image, get_image_format(), VK_IMAGE_ASPECT_DEPTH_BIT,
			VK_IMAGE_VIEW_TYPE_CUBE_ARRAY, layer_count);
		replacement.preview_view = get_graphics_engine().create_image_view(
			replacement.attachment.image, get_image_format(), VK_IMAGE_ASPECT_DEPTH_BIT);

		VkFramebufferCreateInfo info{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
		info.renderPass = render_pass;
		info.attachmentCount = 1;
		info.pAttachments = &replacement.attachment.image_view;
		info.width = extent.width;
		info.height = extent.height;
		info.layers = layer_count;
		if (vkCreateFramebuffer(get_logical_device(), &info, nullptr, &framebuffer) != VK_SUCCESS)
			throw std::runtime_error("Failed to create point-light shadow framebuffer");
	}
	catch (...)
	{
		destroy_frame_images(replacement);
		throw;
	}

	// The caller has waited for this slot's fence. No other slot references it.
	auto& frame = frames.at(frame_idx);
	VkDescriptorImageInfo image_info{shadow_map_sampler, replacement.cube_array_view,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
	VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
	write.dstSet = frame.descriptor;
	write.dstBinding = SDS::RASTERIZATION_SHADOW_MAP_DATA_BINDING;
	write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	write.descriptorCount = 1;
	write.pImageInfo = &image_info;
	vkUpdateDescriptorSets(get_logical_device(), 1, &write, 0, nullptr);
	vkDestroyFramebuffer(get_logical_device(), frame_buffers.at(frame_idx), nullptr);
	destroy_frame_images(frame);
	frame.attachment = replacement.attachment;
	frame.cube_array_view = replacement.cube_array_view;
	frame.preview_view = replacement.preview_view;
	frame.capacity = capacity;
	frame_buffers[frame_idx] = framebuffer;
}

void ShadowMapRenderer::submit_draw_commands(
	VkCommandBuffer command_buffer, VkImageView, const uint32_t frame_index)
{
	const auto& resources = frames.at(frame_index);
	VkRenderPassBeginInfo begin{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
	begin.renderPass = render_pass;
	begin.framebuffer = frame_buffers.at(frame_index);
	begin.renderArea.extent = resources.capacity ? get_extent() : VkExtent2D{1, 1};
	VkClearValue clear{};
	clear.depthStencil = {1.0f, 0};
	begin.clearValueCount = 1;
	begin.pClearValues = &clear;
	vkCmdBeginRenderPass(command_buffer, &begin, VK_SUBPASS_CONTENTS_INLINE);
	reset_draw_state();

	if (get_graphics_engine().get_render_mode() == ERenderMode::RASTERIZED)
	{
		const auto global_set = get_rsrc_mgr().get_global_dset(frame_index);
		vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
			get_graphics_engine().get_pipeline_mgr().get_generic_pipeline_layout(),
			SDS::RASTERIZATION_LOW_FREQ_SET_OFFSET, 1, &global_set, 0, nullptr);
		const auto& frame = get_graphics_engine().get_render_frame();
		for (uint32_t index = 0; index < resources.lighting.light_count; ++index)
		{
			if (resources.lighting.lights[index].shadow_slot < 0)
				continue;
			for (const auto* item : get_graphics_engine().get_draw_lists().shadow())
			{
				if (!item->graphics_renderable->get_visibility()
					|| item->graphics_renderable->get_object_id() == frame.lights[index].object_id)
					continue;
				draw_renderable(command_buffer, *item->renderable,
					item->graphics_renderable->get_frame_dset(frame_index),
					item->graphics_renderable->get_dset(), EPipelineModifier::SHADOW_MAP,
					ERenderType::UNASSIGNED, std::nullopt, index);
			}
		}
	}
	vkCmdEndRenderPass(command_buffer);
}

void ShadowMapRenderer::create_render_pass()
{
	//
	// Attachment
	//
	VkAttachmentDescription shadow_map_attachment{};
	shadow_map_attachment.format = get_image_format();
	shadow_map_attachment.samples = get_msaa_sample_count();
	shadow_map_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	shadow_map_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	shadow_map_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	shadow_map_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	shadow_map_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	shadow_map_attachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	VkAttachmentReference shadow_map_attachment_ref{};
	shadow_map_attachment_ref.attachment = 0;
	shadow_map_attachment_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	//
	// Subpass
	//
	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS; // as opposed to compute subpass
	subpass.colorAttachmentCount = 0;
	subpass.pDepthStencilAttachment = &shadow_map_attachment_ref;

	// render pass
	VkSubpassDependency dependency{};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	// depth image is accessed early in the frament test pipeline stage
	dependency.srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	dependency.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
	dependency.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dependency.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

	VkSubpassDependency dependency2
	{
		0, 
		VK_SUBPASS_EXTERNAL, 
		VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, 
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 
		VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, 
		VK_ACCESS_SHADER_READ_BIT, 
		VK_DEPENDENCY_BY_REGION_BIT 
	};

	std::vector<VkSubpassDependency> dependencies{ dependency, dependency2 };

	std::vector<VkAttachmentDescription> attachments{ shadow_map_attachment };
	VkRenderPassCreateInfo render_pass_create_info{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
	render_pass_create_info.attachmentCount = attachments.size();
	render_pass_create_info.pAttachments = attachments.data();
	render_pass_create_info.subpassCount = 1;
	render_pass_create_info.pSubpasses = &subpass;
	render_pass_create_info.dependencyCount = dependencies.size();
	render_pass_create_info.pDependencies = dependencies.data();
	
	if (vkCreateRenderPass(get_logical_device(), &render_pass_create_info, nullptr, &this->render_pass) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create render pass!");
	}
}

void ShadowMapRenderer::create_sampler()
{
	VkSamplerCreateInfo sampler_info{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
	sampler_info.magFilter = VK_FILTER_LINEAR; // how to interpolate texels that are magnified, solves oversampling
	sampler_info.minFilter = VK_FILTER_LINEAR; // how to interpolate texels that are minimised, solves undersampling
	// U,V,W is convention for texture space dimensions
	sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sampler_info.addressModeV = sampler_info.addressModeU;
	sampler_info.addressModeW = sampler_info.addressModeU;
	sampler_info.anisotropyEnable = false;
	// sampler_info.maxAnisotropy = 1.0f; // 1.0f = no anisotropic filtering
	sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	sampler_info.unnormalizedCoordinates = false; // specifies coordinate system to address texels, in real world this is always true
												  // so that you can use textures of varying resolutions with same coordinates
	sampler_info.compareEnable = false; // if enabled, texels will first be compared to a value and the result of comparison is used in filtering
	sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;
	sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	sampler_info.mipLodBias = 0.0f;
	sampler_info.minLod = 0.0f;
	sampler_info.maxLod = 0.0f;

	if (vkCreateSampler(get_logical_device(), &sampler_info, nullptr, &shadow_map_sampler) != VK_SUCCESS)
	{
		throw std::runtime_error("ShadowMapRenderer: failed to create texture sampler!");
	}	
}
