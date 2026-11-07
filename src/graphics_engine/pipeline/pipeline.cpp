#include "pipeline_manager.ipp"
#include "pipeline.hpp"
#include "pipelines.hpp"
#include "pipelines.ipp"
#include "graphics_engine/graphics_engine.hpp"

#include "game_engine.hpp"
#include "utility.hpp"
#include "resource_paths.hpp"

#include <fmt/core.h>

#include <fstream>
#include <iostream>


static std::vector<char> readFile(const std::string_view filename)
{
    std::ifstream file(filename.data(), std::ios::ate | std::ios::binary);

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

VkShaderModule GraphicsEnginePipeline::create_shader_module(const std::string_view filename) 
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

std::vector<VkVertexInputBindingDescription> GraphicsEnginePipeline::get_binding_descriptions() const
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

	return { binding_description };
}

std::vector<VkVertexInputAttributeDescription> GraphicsEnginePipeline::get_attribute_descriptions() const
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

VkExtent2D GraphicsEnginePipeline::get_extent()
{
	return this->get_graphics_engine().get_extent();
}

VkSampleCountFlagBits GraphicsEnginePipeline::get_msaa_sample_count()
{
	return this->get_graphics_engine().get_msaa_samples();
}

RenderingFormats GraphicsEnginePipeline::get_rendering_formats()
{
	const VkFormat depth_format = get_graphics_engine().find_depth_format();
	return {
		.color = VK_FORMAT_R16G16B16A16_SFLOAT,
		.depth = depth_format,
		.stencil = depth_format,
	};
}

GraphicsEnginePipeline::GraphicsEnginePipeline(GraphicsEngine& engine) :
	GraphicsEngineBaseModule(engine)
{
}

GraphicsEnginePipeline::~GraphicsEnginePipeline()
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

VkPipelineDepthStencilStateCreateInfo GraphicsEnginePipeline::get_depth_stencil_create_info() const
{
	VkPipelineDepthStencilStateCreateInfo depth_stencil_info{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
	depth_stencil_info.depthTestEnable = VK_TRUE; // whether or not new fragments should be compared to depth buffer for incineration
	depth_stencil_info.depthWriteEnable = alpha_mode == EAlphaMode::BLEND ? VK_FALSE : VK_TRUE;
	depth_stencil_info.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL; // comparison, less = closer
	depth_stencil_info.depthBoundsTestEnable = VK_FALSE; // optional bound test so instead of min/max depths we have both
	depth_stencil_info.minDepthBounds = 0.0f;
	depth_stencil_info.maxDepthBounds = 0.0f;
	depth_stencil_info.stencilTestEnable = VK_FALSE; // stencil buffer operationhs

	depth_stencil_info.front.compareMask = UINT32_MAX;
	depth_stencil_info.front.writeMask = UINT32_MAX;
	depth_stencil_info.front.reference = UINT32_MAX;
	depth_stencil_info.front.compareOp = VkCompareOp::VK_COMPARE_OP_ALWAYS;
	depth_stencil_info.front.passOp = VkStencilOp::VK_STENCIL_OP_REPLACE;
	depth_stencil_info.front.failOp = VkStencilOp::VK_STENCIL_OP_KEEP;
	depth_stencil_info.front.depthFailOp = VkStencilOp::VK_STENCIL_OP_KEEP;

	return depth_stencil_info;
}

std::vector<VkDescriptorSetLayout> GraphicsEnginePipeline::get_expected_dset_layouts()
{
	return get_rsrc_mgr().get_rasterization_descriptor_set_layouts();
}

std::vector<VkPushConstantRange> GraphicsEnginePipeline::get_push_constant_ranges() const
{
	VkPushConstantRange range{};
	range.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_GEOMETRY_BIT;
	range.offset = 0;
	range.size = sizeof(SDS::RasterizationPushConstant);
	return { range };
}

void GraphicsEnginePipeline::initialise()
{
	const std::filesystem::path shader_path = ResourcePaths::get_shader(get_shader_name());
	VkPolygonMode polygon_mode = get_polygon_mode();
	// culling is determined by either clockerwise or counter clockwise vertex order
	// RHS uses counter clockwise while LHS (which is our current system) uses clockwise
	VkFrontFace front_face = get_front_face();

	VkShaderModule vertex_shader = create_shader_module((shader_path / "vertex_shader.spv").string());
	VkShaderModule fragment_shader = create_shader_module((shader_path / "fragment_shader.spv").string());
	VkShaderModule geometry_shader = VK_NULL_HANDLE;

	VkPipelineShaderStageCreateInfo vertex_shader_create_info{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
	vertex_shader_create_info.stage = VK_SHADER_STAGE_VERTEX_BIT; // tells vulkan which pipeline stage the shader is going to be used
	vertex_shader_create_info.module = vertex_shader;
	vertex_shader_create_info.pName = "main"; // function to invoke aka entrypoint
	// vertex_shader_create_info.pSpecializationInfo = // allows for specification of shader constants

	VkPipelineShaderStageCreateInfo fragment_shader_create_info{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
	fragment_shader_create_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragment_shader_create_info.module = fragment_shader;
	fragment_shader_create_info.pName = "main";

	std::vector<VkPipelineShaderStageCreateInfo> shader_stages = { vertex_shader_create_info };
	const std::filesystem::path geometry_shader_path = shader_path / "geometry_shader.spv";
	if (std::filesystem::exists(geometry_shader_path))
	{
		geometry_shader = create_shader_module(geometry_shader_path.string());
		VkPipelineShaderStageCreateInfo geometry_shader_create_info{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
		geometry_shader_create_info.stage = VK_SHADER_STAGE_GEOMETRY_BIT;
		geometry_shader_create_info.module = geometry_shader;
		geometry_shader_create_info.pName = "main";
		shader_stages.push_back(geometry_shader_create_info);
	}
	shader_stages.push_back(fragment_shader_create_info);

	VkPipelineLayoutCreateInfo pipeline_layout_create_info{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
	const auto descriptor_set_layouts = get_expected_dset_layouts();
	pipeline_layout_create_info.setLayoutCount = descriptor_set_layouts.size();
	pipeline_layout_create_info.pSetLayouts = descriptor_set_layouts.data();
	std::vector<VkPushConstantRange> push_constant_ranges = get_push_constant_ranges();
	pipeline_layout_create_info.pPushConstantRanges = push_constant_ranges.empty() ? nullptr : push_constant_ranges.data();
	pipeline_layout_create_info.pushConstantRangeCount = push_constant_ranges.size();

	if (vkCreatePipelineLayout(get_logical_device(), &pipeline_layout_create_info, nullptr, &pipeline_layout) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create pipeline layout!");
	}

	//
	// fixed functions
	//

	const auto binding_descriptions = get_binding_descriptions();
	const auto attribute_descriptions = get_attribute_descriptions();
	VkPipelineVertexInputStateCreateInfo vertex_input_create_info{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
	vertex_input_create_info.vertexBindingDescriptionCount = static_cast<uint32_t>(binding_descriptions.size());
	vertex_input_create_info.pVertexBindingDescriptions =
		binding_descriptions.empty() ? nullptr : binding_descriptions.data();
	vertex_input_create_info.vertexAttributeDescriptionCount = static_cast<uint32_t>(attribute_descriptions.size());
	vertex_input_create_info.pVertexAttributeDescriptions = 
		attribute_descriptions.empty() ? nullptr : attribute_descriptions.data();

	// describes what kind of geomertry will be drawn from the vertices and if primitive restart should be enabled
	VkPipelineInputAssemblyStateCreateInfo input_assembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
	input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	input_assembly.primitiveRestartEnable = VK_FALSE;

	VkPipelineViewportStateCreateInfo view_port_state_create_info{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
	view_port_state_create_info.viewportCount = 1;
	view_port_state_create_info.pViewports = nullptr;
	view_port_state_create_info.scissorCount = 1;
	view_port_state_create_info.pScissors = nullptr;
	const VkDynamicState dynamic_states[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
	VkPipelineDynamicStateCreateInfo dynamic_state_create_info{
		VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
	dynamic_state_create_info.dynamicStateCount = static_cast<uint32_t>(std::size(dynamic_states));
	dynamic_state_create_info.pDynamicStates = dynamic_states;

	// takes the geometry shaped by vertices and turns it into fragments to be colored by fragment shader
	VkPipelineRasterizationStateCreateInfo rasterizer_create_info{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO}; 
	rasterizer_create_info.depthClampEnable = VK_FALSE; // true = fragments beyond near and far planes are clamped, false = discarded
	rasterizer_create_info.rasterizerDiscardEnable = VK_FALSE; // if true then geometry never passes through rasterizer stage
	rasterizer_create_info.polygonMode = polygon_mode;
	rasterizer_create_info.lineWidth = 1.0f;
	rasterizer_create_info.cullMode = get_cull_mode(); 
	rasterizer_create_info.frontFace = front_face; // this is the convention
	// rasterizer can alter depth by adding bias (either constant or sloped), can be useful for shadow mapping
	rasterizer_create_info.depthBiasEnable = VK_FALSE;
	rasterizer_create_info.depthBiasConstantFactor = 0.0f;
	rasterizer_create_info.depthBiasClamp = 0.0f;
	rasterizer_create_info.depthBiasSlopeFactor = 0.0f;
	mod_rasterization_state_info(rasterizer_create_info);

	// multisampling is one way of performing anti-aliasing, by combining fragments that lie ontop of the same pixel
	VkPipelineMultisampleStateCreateInfo multisampling_create_info{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
	multisampling_create_info.sampleShadingEnable = VK_FALSE;
	multisampling_create_info.rasterizationSamples = get_msaa_sample_count();
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
	if (alpha_mode == EAlphaMode::BLEND)
	{
		color_blend_attachment.blendEnable = VK_TRUE;
		color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	}
	mod_color_blend_attachment(color_blend_attachment);
	const RenderingFormats formats = get_rendering_formats();
	const bool has_color_attachment = formats.color != VK_FORMAT_UNDEFINED;

	VkPipelineColorBlendStateCreateInfo color_blending_create_info{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
	color_blending_create_info.logicOpEnable = VK_FALSE;
	color_blending_create_info.logicOp = VK_LOGIC_OP_COPY; // Optional
	color_blending_create_info.attachmentCount = has_color_attachment ? 1 : 0;
	color_blending_create_info.pAttachments = has_color_attachment ? &color_blend_attachment : nullptr;
	color_blending_create_info.blendConstants[0] = 0.0f; // Optional
	color_blending_create_info.blendConstants[1] = 0.0f; // Optional
	color_blending_create_info.blendConstants[2] = 0.0f; // Optional
	color_blending_create_info.blendConstants[3] = 0.0f; // Optional

	VkGraphicsPipelineCreateInfo graphics_pipeline_create_info{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
	graphics_pipeline_create_info.stageCount = shader_stages.size();
	graphics_pipeline_create_info.pStages = shader_stages.data();
	graphics_pipeline_create_info.pVertexInputState = &vertex_input_create_info;
	graphics_pipeline_create_info.pInputAssemblyState = &input_assembly;
	graphics_pipeline_create_info.pViewportState = &view_port_state_create_info;
	graphics_pipeline_create_info.pRasterizationState = &rasterizer_create_info;
	graphics_pipeline_create_info.pMultisampleState = &multisampling_create_info;
	graphics_pipeline_create_info.pDepthStencilState = nullptr;
	graphics_pipeline_create_info.pColorBlendState = &color_blending_create_info;
	graphics_pipeline_create_info.pDynamicState = &dynamic_state_create_info;
	auto depth_stencil_create_info = get_depth_stencil_create_info();
	graphics_pipeline_create_info.pDepthStencilState = &depth_stencil_create_info;
	graphics_pipeline_create_info.layout = pipeline_layout;
	VkPipelineRenderingCreateInfo rendering_create_info{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
	const VkFormat color_format = formats.color;
	rendering_create_info.colorAttachmentCount = has_color_attachment ? 1 : 0;
	rendering_create_info.pColorAttachmentFormats = has_color_attachment ? &color_format : nullptr;
	rendering_create_info.depthAttachmentFormat = formats.depth;
	rendering_create_info.stencilAttachmentFormat = formats.stencil;
	graphics_pipeline_create_info.pNext = &rendering_create_info;
	graphics_pipeline_create_info.renderPass = VK_NULL_HANDLE;
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
	if (geometry_shader != VK_NULL_HANDLE)
	{
		vkDestroyShaderModule(get_logical_device(), geometry_shader, nullptr);
	}
}
