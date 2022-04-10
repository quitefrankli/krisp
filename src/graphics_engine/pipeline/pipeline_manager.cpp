#include "pipeline_manager.hpp"
#include "graphics_engine/graphics_engine.hpp"
#include "graphics_engine/graphics_engine_depth_buffer.hpp"

#include <cassert>
#include <iostream>


GraphicsEnginePipelineManager::GraphicsEnginePipelineManager(GraphicsEngine& engine) :
	GraphicsEngineBaseModule(engine)
{
	create_render_pass();

	// pipeline is not copyable and initializer list requires copyable entries
	const auto add_pipeline = [&](ERenderType type)
	{
		pipelines.emplace(type, GraphicsEnginePipeline(engine, type));
	};
	
	add_pipeline(ERenderType::STANDARD);
	add_pipeline(ERenderType::COLOR);
	add_pipeline(ERenderType::WIREFRAME);
	add_pipeline(ERenderType::LIGHT_SOURCE);
}

GraphicsEnginePipelineManager::~GraphicsEnginePipelineManager()
{
	vkDestroyRenderPass(get_logical_device(), render_pass, nullptr);
}

GraphicsEnginePipeline& GraphicsEnginePipelineManager::get_pipeline(ERenderType type)
{
	auto it = pipelines.find(type);
	if (it == pipelines.end())
	{
		throw std::runtime_error("GraphicsEnginePipelineManager::get_pipeline: invalid pipeline type");
	}

	return it->second;
}

VkPipelineLayout& GraphicsEnginePipelineManager::get_main_pipeline_layout()
{ 
	return get_pipeline(ERenderType::STANDARD).pipeline_layout;
}

VkRenderPass GraphicsEnginePipelineManager::get_main_pipeline_render_pass()
{ 
	return render_pass;
}

void GraphicsEnginePipelineManager::create_render_pass()
{
	//
	// Color Attachment
	//
	VkAttachmentDescription color_attachment{};
	color_attachment.format = get_graphics_engine().get_swap_chain().get_image_format();
	color_attachment.samples = VK_SAMPLE_COUNT_1_BIT; // >1 if we are doing multisampling	
	color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; // determine what to do with the data in the attachment before rendering
	color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE; // dtermine what to do with the data in the attachment after rendering
	color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; // specifies which layout the image will have before the render pass begins
	color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR; // specifies the layout to automatically transition to when the render pass finishes

	// subpasses and attachment references
	// a single render pass can consist of multiple subpasses
	VkAttachmentReference color_attachment_ref{};
	color_attachment_ref.attachment = 0; // only works since we only have 1 attachment description
	color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	//
	// Depth Attachment
	//
	VkAttachmentDescription depth_attachment{};
	depth_attachment.format = GraphicsEngineDepthBuffer::findDepthFormat(get_physical_device());
	depth_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
	depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depth_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depth_attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depthAttachmentRef{};
	depthAttachmentRef.attachment = 1;
	depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS; // as opposed to compute subpass
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &color_attachment_ref;
	// subpass.pInputAttachments // attachments that read from a shader
	// subpass.pResolveAttachments // attachments used for multisampling color attachments
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
	VkRenderPassCreateInfo render_pass_create_info{};
	render_pass_create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	render_pass_create_info.attachmentCount = attachments.size();
	render_pass_create_info.pAttachments = attachments.data();
	render_pass_create_info.subpassCount = 1;
	render_pass_create_info.pSubpasses = &subpass;
	render_pass_create_info.dependencyCount = 1;
	render_pass_create_info.pDependencies = &dependency;
	
	if (vkCreateRenderPass(get_logical_device(), &render_pass_create_info, nullptr, &render_pass) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create render pass!");
	}
}