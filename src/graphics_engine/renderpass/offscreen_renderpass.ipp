#include "offscreen_renderpass.hpp"


template<typename GraphicsEngineT>
OffScreenRenderPass<GraphicsEngineT>::OffScreenRenderPass(GraphicsEngineT& engine) :
	GraphicsEngineBaseModule<GraphicsEngineT>(engine)
{
	initialise_render_pass();
	get_graphics_engine().get_graphics_resource_manager().allocate_descriptor_sets_for_raytracing(color_attachment.image_view);
}

template<typename GraphicsEngineT>
OffScreenRenderPass<GraphicsEngineT>::~OffScreenRenderPass()
{
	// vkDestroyFramebuffer(get_graphics_engine().get_logical_device(), frame_buffer, nullptr);
	vkDestroyRenderPass(get_graphics_engine().get_logical_device(), render_pass, nullptr);
}

template<typename GraphicsEngineT>
VkRenderPass OffScreenRenderPass<GraphicsEngineT>::create_render_pass(
	const std::vector<VkFormat>& colorAttachmentFormats,
	VkFormat depthAttachmentFormat,
	VkImageLayout initialLayout,
	VkImageLayout finalLayout)
{
	const bool clear_color = true;
	const bool clear_depth = true;
	const bool has_depth = depthAttachmentFormat != VK_FORMAT_UNDEFINED;

	std::vector<VkAttachmentDescription> allAttachments;
	std::vector<VkAttachmentReference> colorAttachmentRefs;
	for (const auto& format : colorAttachmentFormats)
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
		depthAttachment.format = depthAttachmentFormat;
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

	VkRenderPass render_pass;
	if (vkCreateRenderPass(get_logical_device(), &renderPassInfo, nullptr, &render_pass) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create render pass!");
	}

	return render_pass;
}

template<typename GraphicsEngineT>
void OffScreenRenderPass<GraphicsEngineT>::initialise_render_pass()
{
	const auto offscreen_color_format = VK_FORMAT_R32G32B32A32_SFLOAT;
	const auto depth_format = GraphicsEngineDepthBuffer<GraphicsEngineT>::findDepthFormat(get_physical_device());
	const auto extent = get_graphics_engine().get_extent();

	// create color attachment
	get_graphics_engine().create_image(
		static_cast<uint32_t>(extent.width),
		static_cast<uint32_t>(extent.height),
		offscreen_color_format,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT |
				VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		color_attachment.image,
		color_attachment.image_memory);
	
	color_attachment.image_view = get_graphics_engine().create_image_view(
		color_attachment.image,
		offscreen_color_format,
		VK_IMAGE_ASPECT_COLOR_BIT);

	// m_offscreenColor.descriptor.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

	// Creating the depth buffer
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

	render_pass = create_render_pass({offscreen_color_format}, 
									 depth_format, 
									 VK_IMAGE_LAYOUT_GENERAL, 
									 VK_IMAGE_LAYOUT_GENERAL);
}
