#pragma once

#include "pipeline.hpp"
#include "pipelines.hpp"
#include "graphics_engine/graphics_engine.hpp"
#include "utility.hpp"

#include <fstream>
#include <iostream>
#include "pipeline.hpp"


static std::vector<char> readFile(const std::string_view filename)
{
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open())
	{
        throw std::runtime_error(fmt::format("failed to open file! {}\n", filename));
    }
	
	size_t fileSize = (size_t)file.tellg();
	std::vector<char> buffer(fileSize);
	file.seekg(0);
	file.read(buffer.data(), fileSize);
	file.close();

	return buffer;
}

template<typename GraphicsEngineT>
VkShaderModule GraphicsEnginePipeline<GraphicsEngineT>::create_shader_module(const std::string_view filename) 
{
	const auto code = readFile(filename);
	VkShaderModuleCreateInfo create_info{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
	create_info.codeSize = code.size();
	create_info.pCode = reinterpret_cast<const uint32_t*>(code.data());

	VkShaderModule shader_module;
	if (vkCreateShaderModule(get_logical_device(), &create_info, nullptr, &shader_module) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create shader module!");
	}

	return shader_module;
}

template<typename GraphicsEngineT>
VkVertexInputBindingDescription GraphicsEnginePipeline<GraphicsEngineT>::get_binding_description() const
{
	// describes at which rate to load data from memory thoughout the vertices
	// it specifies the number of bytes between data entries and whether to 
	// move to the next data entry after each vertex or after each instance

	VkVertexInputBindingDescription binding_description{};
	binding_description.binding = 0;
	binding_description.stride = sizeof(SDS::ColorVertex);
	// move to the next data entry after each vertex
	binding_description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	// move to the next data entry after each instance
	// binding_description.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;

	return binding_description;
}

template<typename GraphicsEngineT>
std::vector<VkVertexInputAttributeDescription> GraphicsEnginePipeline<GraphicsEngineT>::get_attribute_descriptions() const
{
	// how to handle the vertex input
	VkVertexInputAttributeDescription position_attr, normal_attr;
	position_attr.binding = 0;
	position_attr.location = 0; // specify in shader
	position_attr.format = VK_FORMAT_R32G32B32_SFLOAT;
	position_attr.offset = offsetof(SDS::ColorVertex, pos);

	normal_attr.binding = 0;
	normal_attr.location = 3;
	normal_attr.format = VK_FORMAT_R32G32B32_SFLOAT;
	normal_attr.offset = offsetof(SDS::ColorVertex, normal);

	return {position_attr, normal_attr};
}

template<typename GraphicsEngineT>
VkExtent2D GraphicsEnginePipeline<GraphicsEngineT>::get_extent()
{
	return get_graphics_engine().get_extent();
}

template<typename GraphicsEngineT>
VkSampleCountFlagBits GraphicsEnginePipeline<GraphicsEngineT>::get_msaa_sample_counts()
{
	return get_graphics_engine().get_msaa_samples();
}

template<typename GraphicsEngineT>
VkRenderPass GraphicsEnginePipeline<GraphicsEngineT>::get_render_pass()
{
	return get_graphics_engine().get_renderer_mgr().
		get_renderer(ERendererType::RASTERIZATION).get_render_pass();
}

template<typename GraphicsEngineT>
std::unique_ptr<GraphicsEnginePipeline<GraphicsEngineT>> GraphicsEnginePipeline<GraphicsEngineT>::create_pipeline(
	GraphicsEngineT& engine,
	EPipelineType type)
{
	std::unique_ptr<GraphicsEnginePipeline> new_pipeline;

	switch (type)
	{
	case EPipelineType::COLOR:
		new_pipeline = std::make_unique<ColorPipeline<GraphicsEngineT>>(engine);
		break;
	case EPipelineType::STANDARD:
		new_pipeline = std::make_unique<TexturePipeline<GraphicsEngineT>>(engine);
		break;
	case EPipelineType::CUBEMAP:
		new_pipeline = std::make_unique<CubemapPipeline<GraphicsEngineT>>(engine);
		break;
	case EPipelineType::RAYTRACING:
		new_pipeline = std::make_unique<RaytracingPipeline<GraphicsEngineT>>(engine);
		break;
	case EPipelineType::LIGHTWEIGHT_OFFSCREEN_PIPELINE:
		new_pipeline = std::make_unique<LightWeightOffscreenPipeline<GraphicsEngineT>>(engine);
		break;
	case EPipelineType::_STENCIL_COLOR_VERTICES:
		new_pipeline = std::make_unique<StencilColorVerticesPipeline<GraphicsEngineT>>(engine);
		break;
	case EPipelineType::_STENCIL_TEXTURE_VERTICES:
		new_pipeline = std::make_unique<StencilTextureVerticesPipeline<GraphicsEngineT>>(engine);
		break;
	case EPipelineType::_WIREFRAME_COLOR_VERTICES:
		new_pipeline = std::make_unique<WireframeColorVerticesPipeline<GraphicsEngineT>>(engine);
		break;
	case EPipelineType::_WIREFRAME_TEXTURE_VERTICES:
		new_pipeline = std::make_unique<WireframeTextureVerticesPipeline<GraphicsEngineT>>(engine);
		break;
	case EPipelineType::STENCIL:
	case EPipelineType::WIREFRAME:
	default:
		throw std::runtime_error("GraphicsEnginePipeline::create_pipeline: invalid pipeline type");
	}

	new_pipeline->initialise();

	return new_pipeline;
}

template<typename GraphicsEngineT>
GraphicsEnginePipeline<GraphicsEngineT>::GraphicsEnginePipeline(GraphicsEngineT& engine) :
	GraphicsEngineBaseModule<GraphicsEngineT>(engine)
{
}

template<typename GraphicsEngineT>
GraphicsEnginePipeline<GraphicsEngineT>::~GraphicsEnginePipeline()
{
	if (graphics_pipeline != VK_NULL_HANDLE)
	{
		vkDestroyPipeline(get_logical_device(), graphics_pipeline, nullptr);
	}
	if (pipeline_layout != VK_NULL_HANDLE)
	{
		vkDestroyPipelineLayout(get_logical_device(), pipeline_layout, nullptr);
	}
}

template<typename GraphicsEngineT>
VkPipelineDepthStencilStateCreateInfo GraphicsEnginePipeline<GraphicsEngineT>::get_depth_stencil_create_info() const
{
	VkPipelineDepthStencilStateCreateInfo depth_stencil_info{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
	depth_stencil_info.depthTestEnable = VK_TRUE; // whether or not new fragments should be compared to depth buffer for incineration
	depth_stencil_info.depthWriteEnable = VK_TRUE; // if new depth of fragments that pass depth test should be written to depth buffer
	depth_stencil_info.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL; // comparison, less = closer
	depth_stencil_info.depthBoundsTestEnable = VK_FALSE; // optional bound test so instead of min/max depths we have both
	depth_stencil_info.minDepthBounds = 0.0f;
	depth_stencil_info.maxDepthBounds = 0.0f;
	depth_stencil_info.stencilTestEnable = VK_TRUE; // stencil buffer operationhs

	depth_stencil_info.front.compareMask = UINT32_MAX;
	depth_stencil_info.front.writeMask = UINT32_MAX;
	depth_stencil_info.front.reference = UINT32_MAX;
	depth_stencil_info.front.compareOp = VkCompareOp::VK_COMPARE_OP_ALWAYS;
	depth_stencil_info.front.passOp = VkStencilOp::VK_STENCIL_OP_REPLACE;
	depth_stencil_info.front.failOp = VkStencilOp::VK_STENCIL_OP_KEEP;
	depth_stencil_info.front.depthFailOp = VkStencilOp::VK_STENCIL_OP_KEEP;

	return depth_stencil_info;
}

template<typename GraphicsEngineT>
void GraphicsEnginePipeline<GraphicsEngineT>::initialise()
{
	std::filesystem::path shader_path = Utility::get().get_shaders_path() / get_shader_name();
	VkPolygonMode polygon_mode = get_polygon_mode();
	// culling is determined by either clockerwise or counter clockwise vertex order
	// RHS uses counter clockwise while LHS (which is our current system) uses clockwise
	VkFrontFace front_face = get_front_face();

	VkShaderModule vertex_shader = create_shader_module(shader_path.string() + "/vertex_shader.spv");
	VkShaderModule fragment_shader = create_shader_module(shader_path.string() + "/fragment_shader.spv");

	VkPipelineShaderStageCreateInfo vertex_shader_create_info{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
	vertex_shader_create_info.stage = VK_SHADER_STAGE_VERTEX_BIT; // tells vulkan which pipeline stage the shader is going to be used
	vertex_shader_create_info.module = vertex_shader;
	vertex_shader_create_info.pName = "main"; // function to invoke aka entrypoint
	// vertex_shader_create_info.pSpecializationInfo = // allows for specification of shader constants

	VkPipelineShaderStageCreateInfo fragment_shader_create_info{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
	fragment_shader_create_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragment_shader_create_info.module = fragment_shader;
	fragment_shader_create_info.pName = "main";

	VkPipelineShaderStageCreateInfo shader_stages[] = { vertex_shader_create_info, fragment_shader_create_info };

	VkPipelineLayoutCreateInfo pipeline_layout_create_info{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
	const auto descriptor_set_layouts = get_rsrc_mgr().get_rasterization_descriptor_set_layouts();
	pipeline_layout_create_info.setLayoutCount = descriptor_set_layouts.size();
	pipeline_layout_create_info.pSetLayouts = descriptor_set_layouts.data();
	pipeline_layout_create_info.pushConstantRangeCount = 0; // Optional
	pipeline_layout_create_info.pPushConstantRanges = nullptr; // Optional

	if (vkCreatePipelineLayout(get_logical_device(), &pipeline_layout_create_info, nullptr, &pipeline_layout) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create pipeline layout!");
	}

	//
	// fixed functions
	//

	const auto binding_description = get_binding_description();
	const auto attribute_descriptions = get_attribute_descriptions();
	VkPipelineVertexInputStateCreateInfo vertex_input_create_info{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
	vertex_input_create_info.vertexBindingDescriptionCount = 1;
	vertex_input_create_info.pVertexBindingDescriptions = &binding_description;
	vertex_input_create_info.vertexAttributeDescriptionCount = static_cast<uint32_t>(attribute_descriptions.size());
	vertex_input_create_info.pVertexAttributeDescriptions = attribute_descriptions.data();

	// describes what kind of geomertry will be drawn from the vertices and if primitive restart should be enabled
	VkPipelineInputAssemblyStateCreateInfo input_assembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
	input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	input_assembly.primitiveRestartEnable = VK_FALSE;

	const VkExtent2D extent = get_extent();
	VkViewport view_port{}; // final output, it defines the transformation from image to framebuffer
	// Vulkan uses right-hand coordinate system, so y is actually pointing down
	// to fix this, we flip the view port upside down
	// https://www.saschawillems.de/blog/2019/03/29/flipping-the-vulkan-viewport/
	view_port.x = 0.0f;
	view_port.y = (float)extent.height;
	view_port.width = (float)extent.width;
	view_port.height = -(float)extent.height;
	view_port.minDepth = 0.0f;
	view_port.maxDepth = 1.0f;

	VkRect2D scissor{}; // any pixels outside scissor is omitted by rasterizer
	scissor.offset = { 0, 0 };
	scissor.extent = extent;

	VkPipelineViewportStateCreateInfo view_port_state_create_info{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
	view_port_state_create_info.viewportCount = 1;
	view_port_state_create_info.pViewports = &view_port;
	view_port_state_create_info.scissorCount = 1;
	view_port_state_create_info.pScissors = &scissor;

	// takes the geometry shaped by vertices and turns it into fragments to be colored by fragment shader
	VkPipelineRasterizationStateCreateInfo rasterizer_create_info{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO}; 
	rasterizer_create_info.depthClampEnable = VK_FALSE; // true = fragments beyond near and far planes are clamped, false = discarded
	rasterizer_create_info.rasterizerDiscardEnable = VK_FALSE; // if true then geometry never passes through rasterizer stage
	rasterizer_create_info.polygonMode = polygon_mode;
	rasterizer_create_info.lineWidth = 1.0f;
	// rasterizer_create_info.cullMode = VK_CULL_MODE_NONE;
	rasterizer_create_info.cullMode = VK_CULL_MODE_BACK_BIT; 
	rasterizer_create_info.frontFace = front_face; // this is the convention
	// rasterizer can alter depth by adding bias (either constant or sloped), can be useful for shadow mapping
	rasterizer_create_info.depthBiasEnable = VK_FALSE;
	rasterizer_create_info.depthBiasConstantFactor = 0.0f;
	rasterizer_create_info.depthBiasClamp = 0.0f;
	rasterizer_create_info.depthBiasSlopeFactor = 0.0f;

	// multisampling is one way of performing anti-aliasing, by combining fragments that lie ontop of the same pixel
	VkPipelineMultisampleStateCreateInfo multisampling_create_info{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
	multisampling_create_info.sampleShadingEnable = VK_FALSE;
	multisampling_create_info.rasterizationSamples = get_msaa_sample_counts();
	multisampling_create_info.minSampleShading = 1.0f;
	multisampling_create_info.pSampleMask = nullptr;
	multisampling_create_info.alphaToCoverageEnable = VK_FALSE;
	multisampling_create_info.alphaToOneEnable = VK_FALSE;

	// after fragment shader returned a color, it must be combined with color already on the framebuffer
	VkPipelineColorBlendAttachmentState color_blend_attachment{};
	color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	color_blend_attachment.blendEnable = VK_FALSE;
	color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
	color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
	color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
	color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;

	VkPipelineColorBlendStateCreateInfo color_blending_create_info{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
	color_blending_create_info.logicOpEnable = VK_FALSE;
	color_blending_create_info.logicOp = VK_LOGIC_OP_COPY; // Optional
	color_blending_create_info.attachmentCount = 1;
	color_blending_create_info.pAttachments = &color_blend_attachment;
	color_blending_create_info.blendConstants[0] = 0.0f; // Optional
	color_blending_create_info.blendConstants[1] = 0.0f; // Optional
	color_blending_create_info.blendConstants[2] = 0.0f; // Optional
	color_blending_create_info.blendConstants[3] = 0.0f; // Optional

	VkGraphicsPipelineCreateInfo graphics_pipeline_create_info{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
	graphics_pipeline_create_info.stageCount = 2;
	graphics_pipeline_create_info.pStages = shader_stages;
	graphics_pipeline_create_info.pVertexInputState = &vertex_input_create_info;
	graphics_pipeline_create_info.pInputAssemblyState = &input_assembly;
	graphics_pipeline_create_info.pViewportState = &view_port_state_create_info;
	graphics_pipeline_create_info.pRasterizationState = &rasterizer_create_info;
	graphics_pipeline_create_info.pMultisampleState = &multisampling_create_info;
	graphics_pipeline_create_info.pDepthStencilState = nullptr;
	graphics_pipeline_create_info.pColorBlendState = &color_blending_create_info;
	graphics_pipeline_create_info.pDynamicState = nullptr;
	auto depth_stencil_create_info = get_depth_stencil_create_info();
	graphics_pipeline_create_info.pDepthStencilState = &depth_stencil_create_info;
	graphics_pipeline_create_info.layout = pipeline_layout;
	graphics_pipeline_create_info.renderPass = get_render_pass();
	graphics_pipeline_create_info.subpass = 0;
	// for derived pipelines can either use base handle OR base index for the
	// index of the pipeline to refer to the base pipeline
	// however it seems this doesn't really optimise anything
	// so we will not do this "optimisation" for now
	// will require setting VK_PIPELINE_CREATE_DERIVATIVE_BIT flag and also another flag in the parent
	graphics_pipeline_create_info.basePipelineHandle = VK_NULL_HANDLE;
	graphics_pipeline_create_info.basePipelineIndex = -1;

	// pipeline cache can be used to significantly speed up pipeline creation
	if (vkCreateGraphicsPipelines(
		get_logical_device(), 
		VK_NULL_HANDLE, 
		1, 
		&graphics_pipeline_create_info, 
		nullptr, 
		&graphics_pipeline) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create graphics pipeline!");
	}

	vkDestroyShaderModule(get_logical_device(), vertex_shader, nullptr);
	vkDestroyShaderModule(get_logical_device(), fragment_shader, nullptr);
}
