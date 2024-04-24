#include "renderers.hpp"
#include "entity_component_system/ecs.hpp"
#include "shared_data_structures.hpp"


ShadowMapRenderer::ShadowMapRenderer(GraphicsEngine& engine) :
	Renderer(engine)
{
	create_render_pass();
	create_sampler();
}

ShadowMapRenderer::~ShadowMapRenderer()
{
	vkDestroySampler(get_logical_device(), shadow_map_sampler, nullptr);
	for (auto& attachment : shadow_map_attachments)
	{
		attachment.destroy(get_logical_device());
	}
	get_rsrc_mgr().free_dsets(shadow_map_dsets);
}

void ShadowMapRenderer::allocate_per_frame_resources(VkImage, VkImageView)
{
	//
	// Generate attachments
	//
	RenderingAttachment shadow_map_attachment;
	const auto extent = this->get_extent();
	const VkFormat depth_format = get_image_format();
	get_graphics_engine().create_image(
		extent.width,
		extent.height,
		depth_format,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		shadow_map_attachment.image,
		shadow_map_attachment.image_memory,
		get_msaa_sample_count());
	shadow_map_attachment.image_view = get_graphics_engine().create_image_view(
		shadow_map_attachment.image, 
		depth_format, 
		VK_IMAGE_ASPECT_DEPTH_BIT);

	create_shadow_map_dset(shadow_map_attachment.image_view);
	shadow_map_attachments.push_back(shadow_map_attachment);

	//
	// Create framebuffer
	//
	std::vector<VkImageView> attachments { 
		shadow_map_attachment.image_view,
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

void ShadowMapRenderer::submit_draw_commands(VkCommandBuffer command_buffer,
                                                                     VkImageView,
                                                                     uint32_t frame_index)
{
	if (get_graphics_engine().is_wireframe_mode)
	{
		return;
	}

	// starting a render pass
	VkRenderPassBeginInfo render_pass_begin_info{};
	render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	render_pass_begin_info.renderPass = this->render_pass;
	render_pass_begin_info.framebuffer = this->frame_buffers[frame_index];
	render_pass_begin_info.renderArea.offset = { 0, 0 };
	render_pass_begin_info.renderArea.extent = this->get_extent();
	
	VkClearValue clear_value = { 1.0f, 0 };
	render_pass_begin_info.clearValueCount = 1;
	render_pass_begin_info.pClearValues = &clear_value;
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
									shape_dsets.size(), // number of sets to bind
									shape_dsets.data(),
									0,
									nullptr);
		
			vkCmdDrawIndexed(
				command_buffer,
				shape.get_num_vertex_indices(),	// vertex count
				1,	// instance count
				total_index_offset,	// first index
				total_vertex_offset,	// first vertex index (used for offsetting and defines the lowest value of gl_VertexIndex)
				0);	// first instance, used as offset for instance rendering, defines the lower value of gl_InstanceIndex

			// this is not get_num_vertex_indices() because we want to offset the vertex set essentially
			// see NOTE:A
			total_vertex_offset += shape.get_num_unique_vertices();
			total_index_offset += shape.get_num_vertex_indices();
		}
	};

	const auto& graphics_objects = get_graphics_engine().get_objects();
	for (const auto& it_pair : graphics_objects)
	{
		const auto& graphics_object = *(it_pair.second);
		if (graphics_object.is_marked_for_delete())
			continue;

		if (!graphics_object.get_visibility())
			continue;

		if (get_graphics_engine().get_ecs().get_light_component(graphics_object.get_id()) != nullptr)
			continue;
			
		const GraphicsEnginePipeline* pipeline = get_graphics_engine().get_pipeline_mgr().fetch_pipeline({ 
			graphics_object.get_render_type(), EPipelineModifier::SHADOW_MAP });
		if (!pipeline)
			continue;
			
		vkCmdBindPipeline(command_buffer, 
							VK_PIPELINE_BIND_POINT_GRAPHICS, 
							pipeline->graphics_pipeline); // bind the graphics pipeline

		per_obj_draw_fn(graphics_object, *pipeline);
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

void ShadowMapRenderer::create_shadow_map_dset(VkImageView shadow_map_view)
{
	shadow_map_dsets.push_back(get_rsrc_mgr().reserve_dset(get_rsrc_mgr().get_shadow_map_dset_layout()));

	VkDescriptorImageInfo image_info{};
	image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	image_info.imageView = shadow_map_view;
	image_info.sampler = shadow_map_sampler;

	VkWriteDescriptorSet combined_image_sampler_descriptor_set{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
	combined_image_sampler_descriptor_set.dstSet = shadow_map_dsets.back();
	combined_image_sampler_descriptor_set.dstBinding = SDS::RASTERIZATION_SHADOW_MAP_DATA_BINDING;
	combined_image_sampler_descriptor_set.dstArrayElement = 0; // offset
	combined_image_sampler_descriptor_set.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	combined_image_sampler_descriptor_set.descriptorCount = 1;
	combined_image_sampler_descriptor_set.pImageInfo = &image_info;
	
	vkUpdateDescriptorSets(get_logical_device(), 1, &combined_image_sampler_descriptor_set, 0, nullptr);
}
