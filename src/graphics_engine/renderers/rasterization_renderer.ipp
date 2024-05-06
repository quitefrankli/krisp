#include "renderers.hpp"
#include "renderable/render_types.hpp"
#include "shared_data_structures.hpp"
#include "graphics_engine/pipeline/pipeline_id.hpp"
#include "graphics_engine/graphics_engine_object.hpp"
#include "graphics_engine/graphics_engine.hpp"
#include "objects/object.hpp"


RasterizationRenderer::RasterizationRenderer(GraphicsEngine& engine) :
	Renderer(engine)
{
	create_render_pass();
}

RasterizationRenderer::~RasterizationRenderer()
{
	for (auto& color_attachment : color_attachments)
	{
		color_attachment.destroy(get_graphics_engine().get_logical_device());
	}
	for (auto& depth_attachment : depth_attachments)
	{
		depth_attachment.destroy(get_graphics_engine().get_logical_device());
	}
	get_rsrc_mgr().free_dsets(shadow_map_dsets);
	vkDestroySampler(this->get_logical_device(), shadow_map_sampler, nullptr);
}

void RasterizationRenderer::allocate_per_frame_resources(VkImage presentation_image, VkImageView presentation_image_view)
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
		VK_IMAGE_ASPECT_DEPTH_BIT);

	color_attachments.push_back(color_attachment);
	depth_attachments.push_back(depth_attachment);

	//
	// Create framebuffer
	//
	std::vector<VkImageView> attachments { 
		color_attachment.image_view, // main attachment color image_view, that shaders write to
		depth_attachment.image_view, // depth buffer image_view
		presentation_image_view // for presentation (msaa resolve is also applied at this step)
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

void RasterizationRenderer::submit_draw_commands(
	VkCommandBuffer command_buffer,
	VkImageView presentation_image_view,
	uint32_t frame_index)
{
	// starting a render pass
	VkRenderPassBeginInfo render_pass_begin_info{};
	render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	render_pass_begin_info.renderPass = this->render_pass;
	render_pass_begin_info.framebuffer = this->frame_buffers[frame_index];
	render_pass_begin_info.renderArea.offset = { 0, 0 };
	render_pass_begin_info.renderArea.extent = this->get_extent();
	
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
							// lets assume that global descriptor objects only use the STANDARD pipeline
							// this is a little dodgy but it seems to be working?
							get_graphics_engine().get_pipeline_mgr().get_generic_pipeline_layout(),
							SDS::RASTERIZATION_LOW_FREQ_SET_OFFSET,
							per_frame_dsets.size(),
							per_frame_dsets.data(),
							0,
							nullptr);
	vkCmdBindDescriptorSets(command_buffer,
							VK_PIPELINE_BIND_POINT_GRAPHICS,
							// lets assume that global descriptor objects only use the STANDARD pipeline
							// this is a little dodgy but it seems to be working?
							get_graphics_engine().get_pipeline_mgr().get_generic_pipeline_layout(),
							SDS::RASTERIZATION_SHADOW_MAP_SET_OFFSET,
							1,
							&shadow_map_dsets[frame_index],
							0,
							nullptr);

	const auto& graphics_objects = get_graphics_engine().get_objects();
	const auto& stenciled_ids = get_graphics_engine().get_stenciled_object_ids();
	for (const auto& [id, obj_ptr] : graphics_objects)
	{
		const auto& graphics_object = *obj_ptr;
		if (graphics_object.is_marked_for_delete())
			continue;

		if (!graphics_object.get_visibility())
			continue;
		
		if (stenciled_ids.find(id) != stenciled_ids.end())
			continue; // skip stenciled objects, we will render them later

		const EPipelineModifier modifier = get_graphics_engine().is_wireframe_mode ? 
			EPipelineModifier::WIREFRAME : EPipelineModifier::NONE;

		draw_object(command_buffer, frame_index, graphics_object, modifier);
	}
	
	// render stenciled objects again, for stencil effect. It's a little costly but at least it uses simpler shader
	for (const auto& id : stenciled_ids)
	{
		const auto it_obj = graphics_objects.find(id);
		if (it_obj == graphics_objects.end())
			continue;

		const auto& graphics_object = *it_obj->second;
		if (graphics_object.is_marked_for_delete())
			continue;

		if (!graphics_object.get_visibility())
			continue;
		
		draw_object(command_buffer, frame_index, graphics_object, EPipelineModifier::STENCIL);
	}

	for (const auto& id : stenciled_ids)
	{
		const auto it_obj = graphics_objects.find(id);
		if (it_obj == graphics_objects.end())
			continue;

		const auto& graphics_object = *it_obj->second;
		if (graphics_object.is_marked_for_delete())
			continue;

		if (!graphics_object.get_visibility())
			continue;
		
		draw_object(command_buffer, frame_index, graphics_object, EPipelineModifier::POST_STENCIL);
	}

	vkCmdEndRenderPass(command_buffer);
}

void RasterizationRenderer::set_shadow_map_inputs(const std::vector<VkImageView>& shadow_map_inputs)
{
	assert(shadow_map_inputs.size() == CSTS::NUM_EXPECTED_SWAPCHAIN_IMAGES);
	
	// create a custom sampler for shadow map, we want to clamp to a white border
	VkSamplerCreateInfo sampler_info{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
	sampler_info.magFilter = VK_FILTER_LINEAR; // how to interpolate texels that are magnified, solves oversampling
	sampler_info.minFilter = VK_FILTER_LINEAR; // how to interpolate texels that are minimised, solves undersampling
	// U,V,W is convention for texture space dimensions
	sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
	sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
	sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
	sampler_info.anisotropyEnable = false;
	sampler_info.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
	sampler_info.unnormalizedCoordinates = false; // specifies coordinate system to address texels, in real world this is always true
												  // so that you can use textures of varying resolutions with same coordinates
	sampler_info.compareEnable = false; // if enabled, texels will first be compared to a value and the result of comparison is used in filtering
	sampler_info.mipLodBias = 0.0f;
	sampler_info.minLod = 0.0f;
	sampler_info.maxLod = 0.0f;

	if (vkCreateSampler(get_logical_device(), &sampler_info, nullptr, &shadow_map_sampler) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create texture sampler!");
	}	

	for (auto input : shadow_map_inputs)
	{
		auto& new_dset = shadow_map_dsets.emplace_back(
			this->get_rsrc_mgr().reserve_dset(this->get_rsrc_mgr().get_shadow_map_dset_layout()));

		VkDescriptorImageInfo image_info{};
		image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		image_info.imageView = input;
		image_info.sampler = shadow_map_sampler;

		VkWriteDescriptorSet combined_sampler_dset{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
		combined_sampler_dset.dstSet = new_dset;
		combined_sampler_dset.dstBinding = 0;
		combined_sampler_dset.dstArrayElement = 0; // offset
		combined_sampler_dset.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		combined_sampler_dset.descriptorCount = 1;
		combined_sampler_dset.pImageInfo = &image_info;

		vkUpdateDescriptorSets(
			get_logical_device(),
			1,
			&combined_sampler_dset,
			0,
			nullptr);
	}	
}

void RasterizationRenderer::create_render_pass()
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
	// color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR; // specifies the layout to automatically transition to when the render pass finishes
	// with multisampled images, we don't want to present them directly, first they have to be resolved
	// to a single regular image
	color_attachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

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
	depth_attachment.samples = this->get_msaa_sample_count();
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
	// Multi-sampling
	//
	VkAttachmentDescription color_attachment_resolve{}; // resolves the multisampled image to a regular image
    color_attachment_resolve.format = get_image_format();
    color_attachment_resolve.samples = VK_SAMPLE_COUNT_1_BIT;
    color_attachment_resolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment_resolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment_resolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment_resolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment_resolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_attachment_resolve.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference color_attachment_resolve_ref{};
	color_attachment_resolve_ref.attachment = 2;
	color_attachment_resolve_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	//
	// Subpass
	//
	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS; // as opposed to compute subpass
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &color_attachment_ref;
	// subpass.pInputAttachments // attachments that read from a shader
	subpass.pResolveAttachments = &color_attachment_resolve_ref;
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

	std::vector<VkAttachmentDescription> attachments{ color_attachment, depth_attachment, color_attachment_resolve };
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