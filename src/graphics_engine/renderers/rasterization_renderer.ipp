#include "renderers.hpp"
#include "renderable/render_types.hpp"
#include "shared_data_structures.hpp"
#include "graphics_engine/pipeline/pipeline_id.hpp"
#include "graphics_engine/graphics_renderable.hpp"
#include "graphics_engine/graphics_engine.hpp"
#include "graphics_engine/dynamic_rendering.hpp"
#include "camera.hpp"
#include "objects/object.hpp"
#include "particle_renderer.ipp"

#include <algorithm>


RasterizationRenderer::RasterizationRenderer(GraphicsEngine& engine) :
	Renderer(engine)
{
}

RasterizationRenderer::~RasterizationRenderer()
{
	for (auto& color_attachment : color_attachments)
	{
		color_attachment.destroy(get_graphics_engine().get_logical_device());
	}
	for (auto& resolve_attachment : resolve_attachments)
	{
		resolve_attachment.destroy(get_graphics_engine().get_logical_device());
	}
	for (auto& depth_attachment : depth_attachments)
	{
		depth_attachment.destroy(get_graphics_engine().get_logical_device());
	}
}

void RasterizationRenderer::allocate_per_frame_resources(VkImage, VkImageView)
{
	//
	// Generate attachments
	//
	RenderingAttachment color_attachment;
	const auto extent = this->get_extent();
	get_graphics_engine().create_image(
		extent.width, 
		extent.height,
		get_image_format(),
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		color_attachment.image,
		color_attachment.image_memory,
		this->get_msaa_sample_count());
	color_attachment.image_view = get_graphics_engine().create_image_view(
		color_attachment.image, 
		get_image_format(),
		VK_IMAGE_ASPECT_COLOR_BIT);

	RenderingAttachment resolve_attachment;
	get_graphics_engine().create_image(
		extent.width,
		extent.height,
		get_image_format(),
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		resolve_attachment.image,
		resolve_attachment.image_memory,
		VK_SAMPLE_COUNT_1_BIT);
	resolve_attachment.image_view = get_graphics_engine().create_image_view(
		resolve_attachment.image,
		get_image_format(),
		VK_IMAGE_ASPECT_COLOR_BIT);

	RenderingAttachment depth_attachment;
	const VkFormat depth_format = get_graphics_engine().find_depth_format();
	get_graphics_engine().create_image(
		extent.width,
		extent.height,
		depth_format,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		depth_attachment.image,
		depth_attachment.image_memory,
		this->get_msaa_sample_count());
	depth_attachment.image_view = get_graphics_engine().create_image_view(
		depth_attachment.image, 
		depth_format, 
		VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);

	color_attachments.push_back(color_attachment);
	resolve_attachments.push_back(resolve_attachment);
	depth_attachments.push_back(depth_attachment);
}

void RasterizationRenderer::submit_draw_commands(
	VkCommandBuffer command_buffer,
	VkImageView presentation_image_view,
	uint32_t frame_index)
{
	const auto extent = get_extent();
	auto& color = color_attachments.at(frame_index);
	auto& resolve = resolve_attachments.at(frame_index);
	auto& depth = depth_attachments.at(frame_index);
	const bool multisampled = get_msaa_sample_count() != VK_SAMPLE_COUNT_1_BIT;
	if (multisampled)
		command_image_barrier(command_buffer, color.image, VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
	command_image_barrier(command_buffer, depth.image,
		VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
		VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
		VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
		VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);
	command_image_barrier(command_buffer, resolve.image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);

	VkRenderingAttachmentInfo color_info{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
	color_info.imageView = multisampled ? color.image_view : resolve.image_view;
	color_info.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	color_info.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	color_info.storeOp = multisampled ? VK_ATTACHMENT_STORE_OP_DONT_CARE : VK_ATTACHMENT_STORE_OP_STORE;
	color_info.clearValue.color = {{0.0f, 0.0f, 0.0f, 1.0f}};
	color_info.resolveMode = multisampled ? VK_RESOLVE_MODE_AVERAGE_BIT : VK_RESOLVE_MODE_NONE;
	color_info.resolveImageView = multisampled ? resolve.image_view : VK_NULL_HANDLE;
	color_info.resolveImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	VkRenderingAttachmentInfo depth_info{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
	depth_info.imageView = depth.image_view;
	depth_info.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	depth_info.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depth_info.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depth_info.clearValue.depthStencil = {1.0f, 0};
	VkRenderingInfo rendering_info{VK_STRUCTURE_TYPE_RENDERING_INFO};
	rendering_info.renderArea = {{0, 0}, extent};
	rendering_info.layerCount = 1;
	rendering_info.colorAttachmentCount = 1;
	rendering_info.pColorAttachments = &color_info;
	rendering_info.pDepthAttachment = &depth_info;
	rendering_info.pStencilAttachment = &depth_info;
	vkCmdBeginRendering(command_buffer, &rendering_info);
	reset_draw_state();

	std::vector<VkDescriptorSet> per_frame_dsets = { 
		get_rsrc_mgr().get_global_dset(frame_index)
	};
	vkCmdBindDescriptorSets(command_buffer,
							VK_PIPELINE_BIND_POINT_GRAPHICS,
							// lets assume that global descriptor objects only use the STANDARD pipeline
							// this is a little dodgy but it seems to be working?
							get_graphics_engine().get_pipeline_mgr().get_generic_pipeline_layout(),
							SDS::RASTERIZATION_LOW_FREQ_SET_OFFSET,
							per_frame_dsets.size(),
							per_frame_dsets.data(),
							0,
							nullptr);
	const auto shadow_dset = static_cast<ShadowMapRenderer&>(
		get_graphics_engine().get_renderer_mgr().get_renderer(ERendererType::SHADOW_MAP))
		.get_shadow_map_dset(frame_index);
	vkCmdBindDescriptorSets(command_buffer,
							VK_PIPELINE_BIND_POINT_GRAPHICS,
							// lets assume that global descriptor objects only use the STANDARD pipeline
							// this is a little dodgy but it seems to be working?
							get_graphics_engine().get_pipeline_mgr().get_generic_pipeline_layout(),
							SDS::RASTERIZATION_SHADOW_MAP_SET_OFFSET,
							1,
							&shadow_dset,
							0,
							nullptr);

	const auto& draw_lists = get_graphics_engine().get_draw_lists();
	const auto& stenciled_ids = get_graphics_engine().get_stenciled_object_ids();
	struct DrawStyle
	{
		EPipelineModifier modifier = EPipelineModifier::NONE;
		std::optional<EShadingMode> shading_override;
	};
	const auto regular_style = [this, &stenciled_ids](const GraphicsDrawItem& item)
	{
		DrawStyle style;
		if (item.renderable->shading_mode == EShadingMode::UNLIT)
			style.shading_override = EShadingMode::UNLIT;
		else if (get_graphics_engine().get_render_mode() == ERenderMode::WIREFRAME)
			style.modifier = EPipelineModifier::WIREFRAME;
		else if (get_graphics_engine().get_render_mode() == ERenderMode::UNLIT_BASE_COLOR)
			style.shading_override = EShadingMode::UNLIT;

		const auto object_id = item.graphics_renderable->get_object_id();
		if (get_graphics_engine().get_render_mode() == ERenderMode::RASTERIZED
			&& object_id && stenciled_ids.contains(*object_id))
			style.modifier = EPipelineModifier::POST_STENCIL;
		return style;
	};
	const auto is_stenciled = [&stenciled_ids](const GraphicsDrawItem& item) {
		const auto object_id = item.graphics_renderable->get_object_id();
		return object_id && stenciled_ids.contains(*object_id);
	};
	const auto skip_regular_draw = [&](const GraphicsDrawItem& item) {
		return !item.graphics_renderable->get_visibility();
	};
	const auto draw_item = [&](const GraphicsDrawItem& item, const DrawStyle style) {
		draw_renderable(
			command_buffer,
			*item.renderable,
			item.graphics_renderable->get_frame_dset(frame_index),
			item.graphics_renderable->get_dset(),
			style.modifier,
			ERenderType::UNASSIGNED,
			style.shading_override);
	};

	for (const GraphicsDrawItem* item : draw_lists.opaque())
	{
		if (skip_regular_draw(*item))
			continue;
		draw_item(*item, regular_style(*item));
	}

	const glm::vec3 camera_position =
		get_graphics_engine().get_render_frame().camera.position;
	const auto blended_order = [&camera_position](
		const GraphicsDrawItem* lhs,
		const GraphicsDrawItem* rhs)
	{
		const float lhs_distance = renderable_distance_squared(
			camera_position,
			lhs->graphics_renderable->get_model_transform());
		const float rhs_distance = renderable_distance_squared(
			camera_position,
			rhs->graphics_renderable->get_model_transform());
		if (lhs_distance != rhs_distance)
			return lhs_distance > rhs_distance;
		return lhs->sort_key < rhs->sort_key;
	};

	std::vector blended_items = draw_lists.blended();
	std::ranges::sort(blended_items, blended_order);
	for (const GraphicsDrawItem* item : blended_items)
		if (!skip_regular_draw(*item))
			draw_item(*item, regular_style(*item));
	
	if (get_graphics_engine().get_render_mode() == ERenderMode::RASTERIZED)
	{
		std::vector<const GraphicsDrawItem*> stencil_items;
		stencil_items.reserve(draw_lists.all().size());
		for (const auto& item : draw_lists.all())
			if (item.graphics_renderable->get_visibility() && is_stenciled(item))
				stencil_items.push_back(&item);
		std::ranges::sort(stencil_items, [](const auto* lhs, const auto* rhs) {
			return lhs->sort_key < rhs->sort_key;
		});

		for (const GraphicsDrawItem* item : stencil_items)
			draw_item(*item, DrawStyle{ .modifier = EPipelineModifier::STENCIL });
	}

	// Render overlay renderables (gizmos etc.) on top after clearing depth.
	{
		VkClearAttachment clear_depth{};
		clear_depth.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		clear_depth.clearValue.depthStencil = { 1.0f, 0 };
		VkClearRect clear_rect{};
		clear_rect.rect.extent = this->get_extent();
		clear_rect.baseArrayLayer = 0;
		clear_rect.layerCount = 1;
		vkCmdClearAttachments(command_buffer, 1, &clear_depth, 1, &clear_rect);

		for (const GraphicsDrawItem* item : draw_lists.overlay_opaque())
			if (!skip_regular_draw(*item))
				draw_item(*item, regular_style(*item));

		std::vector overlay_blended_items = draw_lists.overlay_blended();
		std::ranges::sort(overlay_blended_items, blended_order);
		for (const GraphicsDrawItem* item : overlay_blended_items)
			if (!skip_regular_draw(*item))
				draw_item(*item, regular_style(*item));
	}

	// Render particles within the same rendering scope (skip in wireframe mode)
	if (get_graphics_engine().get_render_mode() == ERenderMode::RASTERIZED)
	{
		auto& particle_renderer = static_cast<ParticleRenderer&>(
			get_graphics_engine().get_renderer_mgr().get_renderer(ERendererType::PARTICLE));
		particle_renderer.render_particles(command_buffer, frame_index);
	}

	vkCmdEndRendering(command_buffer);
	command_image_barrier(command_buffer, resolve.image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT);
}
