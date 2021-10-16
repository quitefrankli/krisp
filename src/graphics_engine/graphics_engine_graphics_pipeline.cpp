#include "graphics_engine.hpp"

#include <vector>
#include <fstream>

//
// static functions
//

static std::vector<char> readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error("failed to open file!");
    }
	
	size_t fileSize = (size_t) file.tellg();
	std::vector<char> buffer(fileSize);
	file.seekg(0);
	file.read(buffer.data(), fileSize);
	file.close();

	return buffer;
}

static VkShaderModule create_shader_module(const std::vector<char>& code, VkDevice& logical_device)
{
	VkShaderModuleCreateInfo create_info{};
	create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	create_info.codeSize = code.size();
	create_info.pCode = reinterpret_cast<const uint32_t*>(code.data());

	VkShaderModule shader_module;
	if (vkCreateShaderModule(logical_device, &create_info, nullptr, &shader_module) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create shader module!");
	}

	return shader_module;
}

//
// graphics engine
// 

void GraphicsEngine::create_graphics_pipeline() {
    auto vertShaderCode = readFile("vertex_shader.spv");
    auto fragShaderCode = readFile("fragment_shader.spv");

	VkShaderModule vertex_shader = create_shader_module(vertShaderCode, get_logical_device());
	VkShaderModule fragment_shader = create_shader_module(fragShaderCode, get_logical_device());

	VkPipelineShaderStageCreateInfo vertex_shader_create_info{};
	vertex_shader_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertex_shader_create_info.stage = VK_SHADER_STAGE_VERTEX_BIT; // tells vulkan which pipeline stage the shader is going to be used
	vertex_shader_create_info.module = vertex_shader;
	vertex_shader_create_info.pName = "main"; // function to invoke aka entrypoint
	// vertex_shader_create_info.pSpecializationInfo = // allows for specification of shader constants

	VkPipelineShaderStageCreateInfo fragment_shader_create_info{};
	fragment_shader_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragment_shader_create_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragment_shader_create_info.module = fragment_shader;
	fragment_shader_create_info.pName = "main";

	VkPipelineShaderStageCreateInfo shader_stages[] = { vertex_shader_create_info, fragment_shader_create_info };

	VkPipelineLayoutCreateInfo pipeline_layout_create_info{};
	pipeline_layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipeline_layout_create_info.setLayoutCount = 1; // for our 1x ubo_descriptor in our descriptor set layout
	pipeline_layout_create_info.pSetLayouts = &descriptor_set_layout;
	pipeline_layout_create_info.pushConstantRangeCount = 0; // Optional
	pipeline_layout_create_info.pPushConstantRanges = nullptr; // Optional

	if (vkCreatePipelineLayout(get_logical_device(), &pipeline_layout_create_info, nullptr, &pipeline_layout) != VK_SUCCESS) {
		throw std::runtime_error("failed to create pipeline layout!");
	}

	//
	// fixed functions
	//

	VkPipelineVertexInputStateCreateInfo vertex_input_create_info{};
	vertex_input_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertex_input_create_info.vertexBindingDescriptionCount = 1;
	vertex_input_create_info.pVertexBindingDescriptions = &binding_description;
	vertex_input_create_info.vertexAttributeDescriptionCount = static_cast<uint32_t>(attribute_descriptions.size());
	vertex_input_create_info.pVertexAttributeDescriptions = attribute_descriptions.data();

	// describes what kind of geomertry will be drawn from the vertices and if primitive restart should be enabled
	VkPipelineInputAssemblyStateCreateInfo input_assembly{};
	input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	input_assembly.primitiveRestartEnable = VK_FALSE;

	auto extent = swap_chain.get_extent();
	VkViewport view_port{}; // final output, it defines the transformation from image to framebuffer
	view_port.x = 0.0f;
	view_port.y = 0.0f;
	view_port.width = extent.width;
	view_port.height = extent.height;
	view_port.minDepth = 0.0f;
	view_port.maxDepth = 1.0f;

	VkRect2D scissor{}; // any pixels outside scissor is omitted by rasterizer
	scissor.offset = { 0, 0 };
	scissor.extent = extent;

	VkPipelineViewportStateCreateInfo view_port_state_create_info{};
	view_port_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	view_port_state_create_info.viewportCount = 1;
	view_port_state_create_info.pViewports = &view_port;
	view_port_state_create_info.scissorCount = 1;
	view_port_state_create_info.pScissors = &scissor;

	VkPipelineRasterizationStateCreateInfo rasterizer_create_info{}; // takes the geometry shaped by vertices and turns it into fragments to be colored by fragment shader
	rasterizer_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer_create_info.depthClampEnable = VK_FALSE; // true = fragments beyond near and far planes are clamped, false = discarded
	rasterizer_create_info.rasterizerDiscardEnable = VK_FALSE; // if true then geometry never passes through rasterizer stage
	rasterizer_create_info.polygonMode = VK_POLYGON_MODE_FILL;
	// rasterizer_create_info.polygonMode = VK_POLYGON_MODE_LINE; // this could be useful for drawing grid lines
	rasterizer_create_info.lineWidth = 1.0f;
	// rasterizer_create_info.cullMode = VK_CULL_MODE_NONE;
	rasterizer_create_info.cullMode = VK_CULL_MODE_BACK_BIT; 
	rasterizer_create_info.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE; //VK_FRONT_FACE_CLOCKWISE; // culling is determined by either clockerwise or counter clockwise vertex order
	// rasterizer can alter depth by adding bias (either constant or sloped), can be useful for shadow mapping
	rasterizer_create_info.depthBiasEnable = VK_FALSE;
	rasterizer_create_info.depthBiasConstantFactor = 0.0f;
	rasterizer_create_info.depthBiasClamp = 0.0f;
	rasterizer_create_info.depthBiasSlopeFactor = 0.0f;

	// multisampling is one way of performing anti-aliasing, by combining fragments that lie ontop of the same pixel
	VkPipelineMultisampleStateCreateInfo multisampling_create_info{};
	multisampling_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling_create_info.sampleShadingEnable = VK_FALSE;
	multisampling_create_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
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

	VkPipelineColorBlendStateCreateInfo color_blending_create_info{};
	color_blending_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	color_blending_create_info.logicOpEnable = VK_FALSE;
	color_blending_create_info.logicOp = VK_LOGIC_OP_COPY; // Optional
	color_blending_create_info.attachmentCount = 1;
	color_blending_create_info.pAttachments = &color_blend_attachment;
	color_blending_create_info.blendConstants[0] = 0.0f; // Optional
	color_blending_create_info.blendConstants[1] = 0.0f; // Optional
	color_blending_create_info.blendConstants[2] = 0.0f; // Optional
	color_blending_create_info.blendConstants[3] = 0.0f; // Optional

	VkGraphicsPipelineCreateInfo graphics_pipeline_create_info{};
	graphics_pipeline_create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
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
	graphics_pipeline_create_info.layout = pipeline_layout;
	graphics_pipeline_create_info.renderPass = get_render_pass();
	graphics_pipeline_create_info.subpass = 0;
	graphics_pipeline_create_info.basePipelineHandle = VK_NULL_HANDLE; // vulkan allows pipeline inheritance, will require setting VK_PIPELINE_CREATE_DERIVATIVE_BIT flag in VkGraphicsPipelineCreateInfo
	graphics_pipeline_create_info.basePipelineIndex = -1;

	// pipeline cache can be used to significantly speed up pipeline creation
	if (vkCreateGraphicsPipelines(get_logical_device(), VK_NULL_HANDLE, 1, &graphics_pipeline_create_info, nullptr, &graphics_engine_pipeline) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create graphics pipeline!");
	}

	vkDestroyShaderModule(get_logical_device(), vertex_shader, nullptr);
	vkDestroyShaderModule(get_logical_device(), fragment_shader, nullptr);
}