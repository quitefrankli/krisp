#include "renderers.hpp"
#include "graphics_engine/resource_manager/graphics_resource_manager.hpp"

#include <mutex>


template<typename GraphicsEngineT>
inline RaytracingRenderer<GraphicsEngineT>::RaytracingRenderer(GraphicsEngineT& engine) :
	Renderer<GraphicsEngineT>(engine)
{
	// create_render_pass();
}

template<typename GraphicsEngineT>
inline RaytracingRenderer<GraphicsEngineT>::~RaytracingRenderer()
{
	for (auto& color_attachment : color_attachments)
	{
		color_attachment.destroy(get_logical_device());
	}
	for (auto& depth_attachment : depth_attachments)
	{
		depth_attachment.destroy(get_logical_device());
	}
	if (!rt_dsets.empty())
	{
		get_rsrc_mgr().free_dsets(rt_dsets);
	}
}

template<typename GraphicsEngineT>
void RaytracingRenderer<GraphicsEngineT>::allocate_per_frame_resources(
	VkImage presentation_image, 
	VkImageView presentation_image_view)
{
	const auto depth_format = get_graphics_engine().find_depth_format();
	const auto extent = this->get_extent();

	// create color attachment
	RenderingAttachment color_attachment;
	get_graphics_engine().create_image(
		static_cast<uint32_t>(extent.width),
		static_cast<uint32_t>(extent.height),
		get_image_format(),
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT |
				VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		color_attachment.image,
		color_attachment.image_memory);
	color_attachment.image_view = get_graphics_engine().create_image_view(
		color_attachment.image,
		get_image_format(),
		VK_IMAGE_ASPECT_COLOR_BIT);

	get_graphics_engine().transition_image_layout(
		color_attachment.image,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_GENERAL);
	// rt_dsets.push_back(create_rt_dset(color_attachment.image_view)); // TODO: uncomment me

	// Creating the depth buffer
	RenderingAttachment depth_attachment;
	get_graphics_engine().create_image(
		static_cast<uint32_t>(extent.width),
		static_cast<uint32_t>(extent.height),
		depth_format,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		depth_attachment.image,
		depth_attachment.image_memory);
	depth_attachment.image_view = get_graphics_engine().create_image_view(
		depth_attachment.image,
		depth_format,
		VK_IMAGE_ASPECT_DEPTH_BIT);

	color_attachments.push_back(color_attachment);
	depth_attachments.push_back(depth_attachment);
	presentation_images.push_back(presentation_image);
}

template<typename GraphicsEngineT>
void RaytracingRenderer<GraphicsEngineT>::submit_draw_commands(
	VkCommandBuffer command_buffer,
	VkImageView presentation_image_view,
	uint32_t frame_index)
{
	if (rt_dsets.empty()) // TODO: delete me
	{
		return;
	}

	// updating the tlas every frame, this could potentially be very expensive!!!
	// TODO: figure out performance costs and whether this is acceptable
	get_graphics_engine().get_raytracing_module().update_tlas2();
	update_rt_dsets();

	vkCmdBindPipeline(
		command_buffer,
		VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
		get_graphics_engine().get_pipeline_mgr().fetch_pipeline({
			EPipelineType::RAYTRACING, EPipelineModifier::NONE })->graphics_pipeline);

	std::vector<VkDescriptorSet> dsets = {
		get_rsrc_mgr().get_global_dset(),
		rt_dsets[frame_index],
		get_rsrc_mgr().get_mesh_data_dset()
	};
	vkCmdBindDescriptorSets(
		command_buffer,
		VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
		get_graphics_engine().get_pipeline_mgr().fetch_pipeline({
			EPipelineType::RAYTRACING, EPipelineModifier::NONE })->pipeline_layout,
		SDS::RAYTRACING_LOW_FREQ_SET_OFFSET,
		dsets.size(),
		dsets.data(),
		0,
		nullptr);
	auto& raytracing_comp = get_graphics_engine().get_raytracing_module();
	LOAD_VK_FUNCTION(vkCmdTraceRaysKHR)(
		command_buffer,
		&raytracing_comp.raygen_sbt_region,
		&raytracing_comp.raymiss_sbt_region,
		&raytracing_comp.rayhit_sbt_region,
		&raytracing_comp.callable_sbt_region,
		static_cast<uint32_t>(this->get_extent().width),
		static_cast<uint32_t>(this->get_extent().height),
		1);
	
	//
	// copy the raytraced image to the swapchain image
	//

	// prepare current swapchain image as transfer destination
	get_graphics_engine().transition_image_layout(
		presentation_images[frame_index],
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		command_buffer);

	// prepare raytraced image as transfer source
	get_graphics_engine().transition_image_layout(
		color_attachments[frame_index].image,
		VK_IMAGE_LAYOUT_GENERAL,
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		command_buffer);

	// copy raytraced image to swapchain image
	const VkExtent2D extent = this->get_extent();
	VkImageBlit region{};
	region.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
	region.srcOffsets[0] = { 0, 0, 0 };
	region.srcOffsets[1] = { static_cast<int32_t>(extent.width), static_cast<int32_t>(extent.height), 1 };
	region.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
	region.dstOffsets[0] = region.srcOffsets[0];
	region.dstOffsets[1] = region.srcOffsets[1];

	vkCmdBlitImage(
		command_buffer,
		color_attachments[frame_index].image,
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		presentation_images[frame_index],
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1,
		&region,
		VK_FILTER_NEAREST);

	// return current swapchain image to presentation layout
	get_graphics_engine().transition_image_layout(
		presentation_images[frame_index],
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
		command_buffer);

	// return raytraced image to general layout, which is the required layout for rendering
	get_graphics_engine().transition_image_layout(
		color_attachments[frame_index].image,
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		VK_IMAGE_LAYOUT_GENERAL,
		command_buffer);
}

template<typename GraphicsEngineT>
void RaytracingRenderer<GraphicsEngineT>::update_rt_dsets()
{
	// technically we don't really need to "free" up the descriptor sets
	// but it's a little less code to just do it this way
	get_rsrc_mgr().free_dsets(rt_dsets);
	rt_dsets.clear();
	for (int i = 0; i < color_attachments.size(); i++)
	{
		rt_dsets.emplace_back(create_rt_dset(color_attachments[i].image_view));
	}
}

template<typename GraphicsEngineT>
void RaytracingRenderer<GraphicsEngineT>::create_render_pass()
{
	const auto depth_format = get_graphics_engine().find_depth_format();
	const std::vector<VkFormat> color_attachment_formats = { get_image_format() };
	const bool clear_color = true;
	const bool clear_depth = true;
	const bool has_depth = depth_format != VK_FORMAT_UNDEFINED;
	const VkImageLayout initialLayout = VK_IMAGE_LAYOUT_GENERAL;
	const VkImageLayout finalLayout = VK_IMAGE_LAYOUT_GENERAL;

	std::vector<VkAttachmentDescription> allAttachments;
	std::vector<VkAttachmentReference> colorAttachmentRefs;
	for (const auto& format : color_attachment_formats)
	{
		VkAttachmentDescription colorAttachment = {};
		colorAttachment.format = format;
		colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		colorAttachment.loadOp = clear_color ? VK_ATTACHMENT_LOAD_OP_CLEAR :
			((initialLayout == VK_IMAGE_LAYOUT_UNDEFINED) ? VK_ATTACHMENT_LOAD_OP_DONT_CARE : VK_ATTACHMENT_LOAD_OP_LOAD);
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		colorAttachment.initialLayout = initialLayout;
		colorAttachment.finalLayout = finalLayout;

		VkAttachmentReference colorAttachmentRef = {};
		colorAttachmentRef.attachment = static_cast<uint32_t>(allAttachments.size());
		colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		allAttachments.push_back(colorAttachment);
		colorAttachmentRefs.push_back(colorAttachmentRef);
	}

	VkAttachmentReference depthAttachmentRef{};
	if (has_depth)
	{
		VkAttachmentDescription depthAttachment = {};
		depthAttachment.format = depth_format;
		depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		depthAttachment.loadOp = clear_depth ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;

		depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depthAttachment.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		depthAttachmentRef.attachment = static_cast<uint32_t>(allAttachments.size());
		depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		allAttachments.push_back(depthAttachment);
	}

	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = static_cast<uint32_t>(colorAttachmentRefs.size());
	subpass.pColorAttachments = colorAttachmentRefs.data();
	subpass.pDepthStencilAttachment = has_depth ? &depthAttachmentRef : VK_NULL_HANDLE;

	VkSubpassDependency dependency{};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcAccessMask = 0;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	VkRenderPassCreateInfo renderPassInfo{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
	renderPassInfo.attachmentCount = static_cast<uint32_t>(allAttachments.size());
	renderPassInfo.pAttachments = allAttachments.data();
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;
	renderPassInfo.dependencyCount = 1;
	renderPassInfo.pDependencies = &dependency;

	if (vkCreateRenderPass(get_logical_device(), &renderPassInfo, nullptr, &this->render_pass) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create render pass!");
	}
}

template<typename GraphicsEngineT>
VkDescriptorSet RaytracingRenderer<GraphicsEngineT>::create_rt_dset(VkImageView rt_image_view)
{
	auto rt_dset = get_rsrc_mgr().reserve_dset(get_rsrc_mgr().get_raytracing_tlas_dset_layout());
	
	VkAccelerationStructureKHR tlas = get_graphics_engine().get_raytracing_module().get_tlas();
	VkWriteDescriptorSetAccelerationStructureKHR descASInfo{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR};
	descASInfo.accelerationStructureCount = 1;
	descASInfo.pAccelerationStructures = &tlas;
	VkDescriptorImageInfo imageInfo{};
	imageInfo.imageView = rt_image_view;
	imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

	VkWriteDescriptorSet tlas_write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
	tlas_write.descriptorCount = 1;
	tlas_write.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
	tlas_write.dstBinding = SDS::RAYTRACING_TLAS_DATA_BINDING;
	tlas_write.dstSet = rt_dset;
	tlas_write.dstArrayElement = 0;
	tlas_write.pNext = &descASInfo;

	VkWriteDescriptorSet out_image_write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
	out_image_write.descriptorCount = 1;
	out_image_write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	out_image_write.dstBinding = SDS::RAYTRACING_OUTPUT_IMAGE_BINDING;
	out_image_write.dstSet = rt_dset;
	out_image_write.dstArrayElement = 0;
	out_image_write.pImageInfo = &imageInfo;

	std::vector<VkWriteDescriptorSet> writes = { tlas_write, out_image_write };

	vkUpdateDescriptorSets(get_logical_device(), static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);

	return rt_dset;
}
