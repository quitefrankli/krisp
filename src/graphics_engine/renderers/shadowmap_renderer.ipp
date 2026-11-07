#include "renderers.hpp"
#include "graphics_engine/dynamic_rendering.hpp"
#include <algorithm>
#include <bit>
#include "shared_data_structures.hpp"


static constexpr uint32_t SHADOW_MAP_LAYER_COUNT = 6; // 6 sided cube

ShadowMapRenderer::ShadowMapRenderer(GraphicsEngine& engine) :
	Renderer(engine)
{
	create_sampler();
}

ShadowMapRenderer::~ShadowMapRenderer()
{
	release_per_frame_resources(true);
	vkDestroySampler(get_logical_device(), shadow_map_sampler, nullptr);
}

void ShadowMapRenderer::destroy_frame_images(FrameResources& frame)
{
	vkDestroyImageView(get_logical_device(), frame.preview_view, nullptr);
	vkDestroyImageView(get_logical_device(), frame.cube_array_view, nullptr);
	frame.attachment.destroy(get_logical_device(), get_graphics_engine().get_allocator());
	frame.preview_view = VK_NULL_HANDLE;
	frame.cube_array_view = VK_NULL_HANDLE;
	frame.attachment = {};
}

void ShadowMapRenderer::allocate_per_frame_resources(VkImage, VkImageView)
{
	if (consume_retained_per_frame_resource())
		return;
	FrameResources frame;
	frame.descriptor = get_rsrc_mgr().reserve_dset(get_rsrc_mgr().get_shadow_map_dset_layout());
	frames.push_back(frame);
	// A tiny cube array remains bindable when the scene has no shadowed lights.
	grow_frame_resources(static_cast<uint32_t>(frames.size() - 1), 0);
}

void ShadowMapRenderer::release_per_frame_resources(const bool image_count_changed)
{
	if (!image_count_changed)
	{
		retain_per_frame_resources(static_cast<uint32_t>(frames.size()));
		return;
	}
	retain_per_frame_resources(0);
	for (auto& frame : frames)
	{
		get_rsrc_mgr().free_dset(frame.descriptor);
		destroy_frame_images(frame);
	}
	frames.clear();
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
	try
	{
		get_graphics_engine().create_image(
			extent.width, extent.height, get_image_format(), VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			replacement.attachment.image, replacement.attachment.allocation,
			get_msaa_sample_count(), layer_count, VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT);
		replacement.attachment.image_view = get_graphics_engine().create_image_view(
			replacement.attachment.image, get_image_format(), VK_IMAGE_ASPECT_DEPTH_BIT,
			VK_IMAGE_VIEW_TYPE_2D_ARRAY, layer_count);
		replacement.cube_array_view = get_graphics_engine().create_image_view(
			replacement.attachment.image, get_image_format(), VK_IMAGE_ASPECT_DEPTH_BIT,
			VK_IMAGE_VIEW_TYPE_CUBE_ARRAY, layer_count);
		replacement.preview_view = get_graphics_engine().create_image_view(
			replacement.attachment.image, get_image_format(), VK_IMAGE_ASPECT_DEPTH_BIT);
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
	destroy_frame_images(frame);
	frame.attachment = replacement.attachment;
	frame.cube_array_view = replacement.cube_array_view;
	frame.preview_view = replacement.preview_view;
	frame.capacity = capacity;
	frame.image_initialized = false;
}

void ShadowMapRenderer::submit_draw_commands(
	VkCommandBuffer command_buffer, VkImageView, const uint32_t frame_index)
{
	auto& resources = frames.at(frame_index);
	const uint32_t layer_count = SHADOW_MAP_LAYER_COUNT * std::max(resources.capacity, 1u);
	if (resources.lighting.shadow_count == 0 && resources.image_initialized)
		return;

	const auto extent = resources.capacity ? get_extent() : VkExtent2D{1, 1};
	command_image_barrier(command_buffer, resources.attachment.image, VK_IMAGE_ASPECT_DEPTH_BIT,
		resources.image_initialized ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		resources.image_initialized ? VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT : VK_PIPELINE_STAGE_2_NONE,
		resources.image_initialized ? VK_ACCESS_2_SHADER_SAMPLED_READ_BIT : VK_ACCESS_2_NONE,
		VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
		VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
		layer_count);
	VkRenderingAttachmentInfo depth_info{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
	depth_info.imageView = resources.attachment.image_view;
	depth_info.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	depth_info.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depth_info.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	depth_info.clearValue.depthStencil = {1.0f, 0};
	VkRenderingInfo rendering_info{VK_STRUCTURE_TYPE_RENDERING_INFO};
	rendering_info.renderArea = {{0, 0}, extent};
	rendering_info.layerCount = layer_count;
	rendering_info.pDepthAttachment = &depth_info;
	vkCmdBeginRendering(command_buffer, &rendering_info);
	command_set_viewport_scissor(command_buffer, extent);
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
	vkCmdEndRendering(command_buffer);
	command_image_barrier(command_buffer, resources.attachment.image, VK_IMAGE_ASPECT_DEPTH_BIT,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
		VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
		VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT, layer_count);
	resources.image_initialized = true;
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
