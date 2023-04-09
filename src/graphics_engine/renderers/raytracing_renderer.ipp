#include "renderers.hpp"
#include "graphics_engine/graphics_resource_manager.hpp"

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
	for (auto& presentation_attachment : presentation_attachments)
	{
		vkDestroyImageView(get_logical_device(), presentation_attachment.image_view, nullptr);
	}
}

template<typename GraphicsEngineT>
void RaytracingRenderer<GraphicsEngineT>::allocate_inflight_frame_resources(VkImage presentation_image)
{
	const auto depth_format = get_graphics_engine().find_depth_format();
	const auto extent = get_graphics_engine().get_extent();

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
	rt_dsets.push_back(create_rt_dset(color_attachment.image_view));

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

	// Create the presentation view
	RenderingAttachment presentation_attachment;
	// NOTE: the below are intentionally commented out,
	// 	presentation is mostly owned by the swapchain
	// presentation_attachment.image = ; 
	// presentation_attachment.image_memory = ;
	presentation_attachment.image_view = get_graphics_engine().create_image_view(
		presentation_image, 
		VK_FORMAT_B8G8R8A8_SRGB,
		VK_IMAGE_ASPECT_COLOR_BIT);
	presentation_attachment.image = presentation_image;

	color_attachments.push_back(color_attachment);
	depth_attachments.push_back(depth_attachment);
	presentation_attachments.push_back(presentation_attachment);
}

template<typename GraphicsEngineT>
void RaytracingRenderer<GraphicsEngineT>::submit_draw_commands(
	VkCommandBuffer command_buffer,
	uint32_t frame_index)
{
	vkCmdBindPipeline(
		command_buffer,
		VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
		get_graphics_engine().get_pipeline_mgr().get_pipeline(EPipelineType::RAYTRACING).graphics_pipeline);
	vkCmdBindDescriptorSets(
		command_buffer,
		VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
		get_graphics_engine().get_pipeline_mgr().get_pipeline(EPipelineType::RAYTRACING).pipeline_layout,
		0,
		1,
		&rt_dsets[frame_index],
		0,
		nullptr);
	GraphicsEngineRayTracing<GraphicsEngineT>& raytracing_comp = get_graphics_engine().get_raytracing();
	LOAD_VK_FUNCTION(vkCmdTraceRaysKHR)(
		command_buffer,
		&raytracing_comp.raygen_sbt_region,
		&raytracing_comp.raymiss_sbt_region,
		&raytracing_comp.rayhit_sbt_region,
		&raytracing_comp.callable_sbt_region,
		static_cast<uint32_t>(get_graphics_engine().get_extent().width),
		static_cast<uint32_t>(get_graphics_engine().get_extent().height),
		1);
	
	//
	// copy the raytraced image to the swapchain image
	//

	// prepare current swapchain image as transfer destination
	get_graphics_engine().transition_image_layout(
		presentation_attachments[frame_index].image,
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
	const VkExtent2D extent = get_graphics_engine().get_extent();
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
		presentation_attachments[frame_index].image,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1,
		&region,
		VK_FILTER_NEAREST);
	// vkCmdCopyImage(
	// 	command_buffer, 
	// 	color_attachments[frame_index].image, 
	// 	VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 
	// 	presentation_attachments[frame_index].image, 
	// 	VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 
	// 	1, 
	// 	&copy_region);

	// return current swapchain image to presentation layout
	get_graphics_engine().transition_image_layout(
		presentation_attachments[frame_index].image,
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

	if (vkCreateRenderPass(get_logical_device(), &renderPassInfo, nullptr, &render_pass) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create render pass!");
	}
}

template<typename GraphicsEngineT>
VkDescriptorSet RaytracingRenderer<GraphicsEngineT>::create_rt_dset(VkImageView rt_image_view)
{
	RaytracingResources& ray_tracing_resources = 
		get_graphics_engine().get_graphics_resource_manager().get_raytracing_resources();
	auto rt_dset = get_graphics_engine().get_graphics_resource_manager().reserve_raytracing_dsets(1)[0];

	VkAccelerationStructureKHR tlas{};
	VkWriteDescriptorSetAccelerationStructureKHR descASInfo{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR};
	descASInfo.accelerationStructureCount = 1;
	descASInfo.pAccelerationStructures = &tlas;
	VkDescriptorImageInfo imageInfo{};
	imageInfo.imageView = rt_image_view;
	imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

	VkWriteDescriptorSet tlas_write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
	tlas_write.descriptorCount = 1;
	tlas_write.descriptorType = ray_tracing_resources.tlas_binding.descriptorType;
	tlas_write.dstBinding = ray_tracing_resources.tlas_binding.binding;
	tlas_write.dstSet = rt_dset;
	tlas_write.dstArrayElement = 0;
	tlas_write.pNext = &descASInfo;

	VkWriteDescriptorSet out_image_write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
	out_image_write.descriptorCount = 1;
	out_image_write.descriptorType = ray_tracing_resources.output_binding.descriptorType;
	out_image_write.dstBinding = ray_tracing_resources.output_binding.binding;
	out_image_write.dstSet = rt_dset;
	out_image_write.dstArrayElement = 0;
	out_image_write.pImageInfo = &imageInfo;

	std::vector<VkWriteDescriptorSet> writes = { tlas_write, out_image_write };

	vkUpdateDescriptorSets(get_logical_device(), static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);

	return rt_dset;
}
