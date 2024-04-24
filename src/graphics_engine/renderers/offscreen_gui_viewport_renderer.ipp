#include "renderers.hpp"
#include "shared_data_structures.hpp"

#include <ImGui/imgui_impl_vulkan.h>


OffscreenGuiViewportRenderer::OffscreenGuiViewportRenderer(GraphicsEngine& engine) :
	Renderer(engine)
{
	create_render_pass();
}

OffscreenGuiViewportRenderer::~OffscreenGuiViewportRenderer()
{
	for (auto& color_attachment : color_attachments)
		color_attachment.destroy(get_graphics_engine().get_logical_device());
	for (auto& depth_attachment : depth_attachments)
		depth_attachment.destroy(get_graphics_engine().get_logical_device());
}

void OffscreenGuiViewportRenderer::allocate_per_frame_resources(VkImage, VkImageView)
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

	RenderingAttachment depth_attachment;
	const VkFormat depth_format = get_graphics_engine().find_depth_format();
	get_graphics_engine().create_image(
		extent.width,
		extent.height,
		depth_format,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		depth_attachment.image,
		depth_attachment.image_memory,
		get_msaa_sample_count());
	depth_attachment.image_view = get_graphics_engine().create_image_view(
		depth_attachment.image, 
		depth_format, 
		VK_IMAGE_ASPECT_DEPTH_BIT);

	color_attachments.push_back(color_attachment);
	depth_attachments.push_back(depth_attachment);

	//
	// Create framebuffer
	//
	std::vector<VkImageView> attachments { 
		color_attachment.image_view, // main attachment color image_view, that shaders write to
		depth_attachment.image_view // depth buffer image_view
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

void OffscreenGuiViewportRenderer::submit_draw_commands(VkCommandBuffer command_buffer,
																		 VkImageView,
																		 uint32_t frame_index)
{
	const auto& graphics_objects = get_graphics_engine().get_offscreen_rendering_objects();
	if (graphics_objects.empty())
	{
		return; // nothing to draw, but maybe we should still keep this so it gets refreshed?
	}

	// starting a render pass
	VkRenderPassBeginInfo render_pass_begin_info{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
	render_pass_begin_info.renderPass = this->render_pass;
	render_pass_begin_info.framebuffer = this->frame_buffers[frame_index];
	render_pass_begin_info.renderArea.offset = { 0, 0 };
	render_pass_begin_info.renderArea.extent = get_extent();
	
	std::vector<VkClearValue> clear_values(2);
	clear_values[0].color = { 0.0f, 0.0f, 0.0f, 1.0f };
	clear_values[1].depthStencil = { 1.0f, 0 };
	render_pass_begin_info.clearValueCount = clear_values.size();
	render_pass_begin_info.pClearValues = clear_values.data();
	vkCmdBeginRenderPass(command_buffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

	std::vector<VkDescriptorSet> per_frame_dsets = { 
		get_rsrc_mgr().get_global_dset(frame_index)
	};
	vkCmdBindDescriptorSets(command_buffer,
							VK_PIPELINE_BIND_POINT_GRAPHICS,
							get_graphics_engine().get_pipeline_mgr().get_generic_pipeline_layout(),
							SDS::RASTERIZATION_LOW_FREQ_SET_OFFSET,
							per_frame_dsets.size(),
							per_frame_dsets.data(),
							0,
							nullptr);

	const auto per_obj_draw_fn = [&](
		const GraphicsEngineObject& object, 
		const GraphicsEnginePipeline& pipeline)
	{
		// NOTE:A the vertex and index buffers contain the data for all the 'vertex_sets/shapes' concatenated together
		
		VkDeviceSize buffer_offset = get_rsrc_mgr().get_vertex_buffer_offset(object.get_id());
		VkBuffer buffer = get_rsrc_mgr().get_vertex_buffer();
		vkCmdBindVertexBuffers(
			command_buffer, 
			0, 										// first buffer in vertex buffers array
			1, 										// number of vertex buffers
			&buffer, 								// array of vertex buffers to bind
			&buffer_offset							// byte offset to start from for each buffer
		);
		vkCmdBindIndexBuffer(
			command_buffer,
			get_rsrc_mgr().get_index_buffer(),
			get_rsrc_mgr().get_index_buffer_offset(object.get_id()),
			VK_INDEX_TYPE_UINT32
		);

		std::vector<VkDescriptorSet> object_dsets = { object.get_dset(frame_index) };
		vkCmdBindDescriptorSets(
			command_buffer,
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			pipeline.pipeline_layout,
			SDS::RASTERIZATION_HIGH_FREQ_PER_OBJ_SET_OFFSET,
			object_dsets.size(),
			object_dsets.data(),
			0,
			nullptr);

		// this really should be per object, we will adjust in the future
		int total_vertex_offset = 0;
		int total_index_offset = 0;
		for (const auto& shape : object.get_shapes())
		{
			// descriptor binding, we need to bind the descriptor set for each swap chain image and for each vertex_set with different descriptor set
			std::vector<VkDescriptorSet> shape_dsets = { shape.get_dset() };
			vkCmdBindDescriptorSets(command_buffer, 
									VK_PIPELINE_BIND_POINT_GRAPHICS, // unlike vertex buffer, descriptor sets are not unique to the graphics pipeline, compute pipeline is also possible
									pipeline.pipeline_layout, 
									SDS::RASTERIZATION_HIGH_FREQ_PER_SHAPE_SET_OFFSET, // offset
									shape_dsets.size(),
									shape_dsets.data(),
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

	for (const auto& it_pair : graphics_objects)
	{
		const auto& graphics_object = *(it_pair.second);
		if (graphics_object.is_marked_for_delete())
			continue;

		// if (!graphics_object.get_visibility())
			// continue;
		
		// only support color and texture render types for now
		if (graphics_object.get_render_type() != EPipelineType::COLOR && 
			graphics_object.get_render_type() != EPipelineType::STANDARD)
			continue;

		GraphicsEnginePipeline& pipeline = *get_graphics_engine().get_pipeline_mgr().fetch_pipeline(
			{ EPipelineType::LIGHTWEIGHT_OFFSCREEN_PIPELINE, EPipelineModifier::NONE });
		vkCmdBindPipeline(
			command_buffer, 
			VK_PIPELINE_BIND_POINT_GRAPHICS, 
			pipeline.graphics_pipeline);

		per_obj_draw_fn(graphics_object, pipeline);
	}
	
	vkCmdEndRenderPass(command_buffer);
}

VkExtent2D OffscreenGuiViewportRenderer::get_extent()
{
	// remember to keep aspect ratio the same as default window
	const VkExtent2D main_extent = Renderer::get_extent();
	return VkExtent2D
	{ 
		static_cast<uint32_t>(main_extent.width * 0.33f), 
		static_cast<uint32_t>(main_extent.height * 0.33f)	
	};
}

void OffscreenGuiViewportRenderer::create_render_pass()
{
	//
	// Color Attachment
	//
	VkAttachmentDescription color_attachment{}; // main attachment that our shaders write to
	color_attachment.format = get_image_format();
	color_attachment.samples = get_msaa_sample_count(); // number of samples to write for each pixel
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
	// Depth Attachment
	//
	VkAttachmentDescription depth_attachment{};
	depth_attachment.format = get_graphics_engine().find_depth_format();
	depth_attachment.samples = get_msaa_sample_count();
	depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depth_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depth_attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depthAttachmentRef{};
	depthAttachmentRef.attachment = 1;
	depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	//
	// Subpass
	//
	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS; // as opposed to compute subpass
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &color_attachment_ref;
	// subpass.pInputAttachments // attachments that read from a shader
	// subpass.pResolveAttachments = &color_attachment_resolve_ref;
	subpass.pDepthStencilAttachment = &depthAttachmentRef;
	// subpass.pPreserveAttachments // attachments that are not used by this subpass, but for which the data must be preserved

	// render pass
	VkSubpassDependency dependency{};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	// depth image is accessed early in the frament test pipeline stage
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dependency.srcAccessMask = 0;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

	std::vector<VkAttachmentDescription> attachments{ color_attachment, depth_attachment };
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
