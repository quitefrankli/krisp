#pragma once

#include "pipelines.hpp"


template<typename GraphicsEngineT>
VkVertexInputBindingDescription TexturePipeline<GraphicsEngineT>::get_binding_description() const
{
	VkVertexInputBindingDescription binding_description{};
	binding_description.binding = 0;
	binding_description.stride = sizeof(SDS::TexVertex);
	binding_description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	return binding_description;
}

template<typename GraphicsEngineT>
std::vector<VkVertexInputAttributeDescription> TexturePipeline<GraphicsEngineT>::get_attribute_descriptions() const
{
	VkVertexInputAttributeDescription position_attr, texCoord_attr, normal_attr;
	position_attr.binding = 0;
	position_attr.location = 0; // specify in shader
	position_attr.format = VK_FORMAT_R32G32B32_SFLOAT;
	position_attr.offset = offsetof(SDS::TexVertex, pos);

	texCoord_attr.binding = 0;
	texCoord_attr.location = 2; // specify in shader
	texCoord_attr.format = VK_FORMAT_R32G32_SFLOAT;
	texCoord_attr.offset = offsetof(SDS::TexVertex, texCoord);

	normal_attr.binding = 0;
	normal_attr.location = 3;
	normal_attr.format = VK_FORMAT_R32G32B32_SFLOAT;
	normal_attr.offset = offsetof(SDS::TexVertex, normal);

	return {position_attr, texCoord_attr, normal_attr};
}

template<typename GraphicsEngineT>
VkVertexInputBindingDescription CubemapPipeline<GraphicsEngineT>::get_binding_description() const
{
	VkVertexInputBindingDescription binding_description{};
	binding_description.binding = 0;
	binding_description.stride = sizeof(SDS::TexVertex);
	binding_description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	return binding_description;
}

template<typename GraphicsEngineT>
std::vector<VkVertexInputAttributeDescription> CubemapPipeline<GraphicsEngineT>::get_attribute_descriptions() const
{
	VkVertexInputAttributeDescription position_attr, texCoord_attr, normal_attr;
	position_attr.binding = 0;
	position_attr.location = 0; // specify in shader
	position_attr.format = VK_FORMAT_R32G32B32_SFLOAT;
	position_attr.offset = offsetof(SDS::TexVertex, pos);

	texCoord_attr.binding = 0;
	texCoord_attr.location = 2; // specify in shader
	texCoord_attr.format = VK_FORMAT_R32G32_SFLOAT;
	texCoord_attr.offset = offsetof(SDS::TexVertex, texCoord);

	return {position_attr, texCoord_attr};
}

template<typename GraphicsEngineT>
template<typename VertexType>
VkVertexInputBindingDescription StencilPipeline<GraphicsEngineT>::_get_binding_description()
{
	VkVertexInputBindingDescription binding_description{};
	binding_description.binding = 0;
	binding_description.stride = sizeof(VertexType);
	binding_description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	return binding_description;
}

template<typename GraphicsEngineT>
template<typename VertexType>
std::vector<VkVertexInputAttributeDescription> StencilPipeline<GraphicsEngineT>::_get_attribute_descriptions()
{
	VkVertexInputAttributeDescription position_attr;
	position_attr.binding = 0;
	position_attr.location = 0; // specify in shader
	position_attr.format = VK_FORMAT_R32G32B32_SFLOAT;
	position_attr.offset = offsetof(VertexType, pos);

	return {position_attr};
}

template<typename GraphicsEngineT>
VkPipelineDepthStencilStateCreateInfo CubemapPipeline<GraphicsEngineT>::get_depth_stencil_create_info() const
{
	VkPipelineDepthStencilStateCreateInfo info = GraphicsEnginePipeline<GraphicsEngineT>::get_depth_stencil_create_info();
	info.stencilTestEnable = VK_FALSE;

	return info;
}

template<typename GraphicsEngineT>
inline VkPipelineDepthStencilStateCreateInfo StencilPipeline<GraphicsEngineT>::get_depth_stencil_create_info() const
{
	VkPipelineDepthStencilStateCreateInfo info = GraphicsEnginePipeline<GraphicsEngineT>::get_depth_stencil_create_info();
	// render all objects twice
	// on first render always succeed and write 1 to stencil buffer
	// on second render only render if stencil buffer DOES NOT have 1 and don't write to buffer
	info.front.compareOp = VkCompareOp::VK_COMPARE_OP_NOT_EQUAL;
	info.front.passOp = VkStencilOp::VK_STENCIL_OP_KEEP;
	info.front.failOp = VkStencilOp::VK_STENCIL_OP_KEEP;
	info.front.depthFailOp = VkStencilOp::VK_STENCIL_OP_KEEP;


	info.front = [&]() {
		VkStencilOpState stencil_op_state{};
		stencil_op_state.compareMask = UINT32_MAX;
		stencil_op_state.writeMask = UINT32_MAX;
		stencil_op_state.reference = UINT32_MAX;

		// render all objects twice
		// on first render always succeed and write 1 to stencil buffer
		// on second render only render if stencil buffer DOES NOT have 1 and don't write to buffer
		// stencil_op_state.compareOp = VkCompareOp::VK_COMPARE_OP_ALWAYS;
		stencil_op_state.compareOp = VkCompareOp::VK_COMPARE_OP_NOT_EQUAL;
		// stencil_op_state.compareOp = VkCompareOp::VK_COMPARE_OP_EQUAL;
		stencil_op_state.passOp = VkStencilOp::VK_STENCIL_OP_KEEP;
		stencil_op_state.failOp = VkStencilOp::VK_STENCIL_OP_KEEP;
		stencil_op_state.depthFailOp = VkStencilOp::VK_STENCIL_OP_KEEP;

		return stencil_op_state;
	}();
	return info;
}

template<typename GraphicsEngineT>
template<typename VertexType>
VkVertexInputBindingDescription WireframePipeline<GraphicsEngineT>::_get_binding_description()
{
	VkVertexInputBindingDescription binding_description{};
	binding_description.binding = 0;
	binding_description.stride = sizeof(VertexType);
	binding_description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	return binding_description;
}

template<typename GraphicsEngineT>
template<typename VertexType>
std::vector<VkVertexInputAttributeDescription> WireframePipeline<GraphicsEngineT>::_get_attribute_descriptions()
{
	VkVertexInputAttributeDescription position_attr;
	position_attr.binding = 0;
	position_attr.location = 0; // specify in shader
	position_attr.format = VK_FORMAT_R32G32B32_SFLOAT;
	position_attr.offset = offsetof(VertexType, pos);

	return {position_attr};
}

template<typename GraphicsEngineT>
void RaytracingPipeline<GraphicsEngineT>::initialise()
{
	std::filesystem::path shader_path = Utility::get().get_shaders_path() / get_shader_name();
	VkShaderModule raygen_shader = create_shader_module(shader_path.string() + "/raygen_shader.spv");
	VkShaderModule rayhit_shader = create_shader_module(shader_path.string() + "/rayhit_shader.spv");
	VkShaderModule raymiss_shader = create_shader_module(shader_path.string() + "/raymiss_shader.spv");

	VkPipelineShaderStageCreateInfo raygen_info{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
	raygen_info.stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
	raygen_info.module = raygen_shader;
	raygen_info.pName = "main"; // function to invoke aka entrypoint

	VkPipelineShaderStageCreateInfo raymiss_info{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
	raymiss_info.stage = VK_SHADER_STAGE_MISS_BIT_KHR;
	raymiss_info.module = raymiss_shader;
	raymiss_info.pName = "main";

	VkPipelineShaderStageCreateInfo rayhit_info{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
	rayhit_info.stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
	rayhit_info.module = rayhit_shader;
	rayhit_info.pName = "main";

	std::vector<VkPipelineShaderStageCreateInfo> shader_stages = { raygen_info, raymiss_info, rayhit_info };

	VkRayTracingShaderGroupCreateInfoKHR rt_shader_group_info{VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR};
	rt_shader_group_info.generalShader = VK_SHADER_UNUSED_KHR;
	rt_shader_group_info.closestHitShader = VK_SHADER_UNUSED_KHR;
	rt_shader_group_info.anyHitShader = VK_SHADER_UNUSED_KHR;
	rt_shader_group_info.intersectionShader = VK_SHADER_UNUSED_KHR;

	// raygen shader
	rt_shader_group_info.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
	rt_shader_group_info.generalShader = 0; // index of the shader in the shader stage array
	shader_groups.push_back(rt_shader_group_info);
	rt_shader_group_info.generalShader = VK_SHADER_UNUSED_KHR; // reset to unused

	// raymiss shader
	rt_shader_group_info.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
	rt_shader_group_info.generalShader = 1;
	shader_groups.push_back(rt_shader_group_info);
	rt_shader_group_info.generalShader = VK_SHADER_UNUSED_KHR; // reset to unused

	// rayhit shader
	// this is for triangles only, if we were to use procedual geometry we would need to use intersection shaders
	// i.e. this might be useful for spheres
	rt_shader_group_info.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
	rt_shader_group_info.closestHitShader = 2;
	shader_groups.push_back(rt_shader_group_info);

	VkPipelineLayoutCreateInfo pipeline_layout_create_info{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
	const auto descriptor_set_layouts = get_rsrc_mgr().get_raytracing_descriptor_set_layouts();
	pipeline_layout_create_info.setLayoutCount = descriptor_set_layouts.size();
	pipeline_layout_create_info.pSetLayouts = descriptor_set_layouts.data();
	pipeline_layout_create_info.pushConstantRangeCount = 0; // Optional
	pipeline_layout_create_info.pPushConstantRanges = nullptr; // Optional

	if (vkCreatePipelineLayout(get_logical_device(), &pipeline_layout_create_info, nullptr, &pipeline_layout) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create pipeline layout!");
	}

	VkRayTracingPipelineCreateInfoKHR raytracing_pipeline_create_info{VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR};
	raytracing_pipeline_create_info.stageCount = shader_stages.size();
	raytracing_pipeline_create_info.pStages = shader_stages.data();
	raytracing_pipeline_create_info.groupCount = shader_groups.size();
	raytracing_pipeline_create_info.pGroups = shader_groups.data();
	// max recursion depth for raytracing
	// note that this can be quite expensive, it's ideal to keep it to 1 and instead use an iterative approach
	raytracing_pipeline_create_info.maxPipelineRayRecursionDepth = 1;
	raytracing_pipeline_create_info.layout = pipeline_layout;

	if (LOAD_VK_FUNCTION(vkCreateRayTracingPipelinesKHR)(
		get_logical_device(), 
		VK_NULL_HANDLE, 
		VK_NULL_HANDLE, 
		1, 
		&raytracing_pipeline_create_info, 
		nullptr, 
		&graphics_pipeline) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create raytracing pipeline!");
	}

	vkDestroyShaderModule(get_logical_device(), raygen_shader, nullptr);
	vkDestroyShaderModule(get_logical_device(), rayhit_shader, nullptr);
	vkDestroyShaderModule(get_logical_device(), raymiss_shader, nullptr);
}

template<typename GraphicsEngineT>
VkRenderPass LightWeightOffscreenPipeline<GraphicsEngineT>::get_render_pass()
{
	return get_graphics_engine().get_renderer_mgr().
		get_renderer(ERendererType::OFFSCREEN_GUI_VIEWPORT).get_render_pass();
}

template<typename GraphicsEngineT>
VkExtent2D LightWeightOffscreenPipeline<GraphicsEngineT>::get_extent()
{
	return get_graphics_engine().get_renderer_mgr().
		get_renderer(ERendererType::OFFSCREEN_GUI_VIEWPORT).get_extent();	
}

template<typename GraphicsEngineT>
VkVertexInputBindingDescription SkinnedPipeline<GraphicsEngineT>::get_binding_description() const
{
	VkVertexInputBindingDescription binding_description{};
	binding_description.binding = 0;
	binding_description.stride = sizeof(SDS::TexVertex);
	binding_description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	return binding_description;
}

template<typename GraphicsEngineT>
std::vector<VkVertexInputAttributeDescription> SkinnedPipeline<GraphicsEngineT>::get_attribute_descriptions() const
{	
	VkVertexInputAttributeDescription position_attr{};
	position_attr.binding = 0;
	position_attr.location = 0;
	position_attr.format = VK_FORMAT_R32G32B32_SFLOAT;
	position_attr.offset = offsetof(SDS::SkinnedVertex, pos);

	VkVertexInputAttributeDescription normal_attr{};
	normal_attr.binding = 0;
	normal_attr.location = 1;
	normal_attr.format = VK_FORMAT_R32G32B32_SFLOAT;
	normal_attr.offset = offsetof(SDS::SkinnedVertex, normal);

	VkVertexInputAttributeDescription texCoord_attr{};
	texCoord_attr.binding = 0;
	texCoord_attr.location = 2;
	texCoord_attr.format = VK_FORMAT_R32G32_SFLOAT;
	texCoord_attr.offset = offsetof(SDS::SkinnedVertex, texCoord);

	VkVertexInputAttributeDescription bone_ids_attr{};
	bone_ids_attr.binding = 0;
	bone_ids_attr.location = 3;
	bone_ids_attr.format = VK_FORMAT_R32G32B32_SFLOAT; // using float as it's more convenient, we can simply refer to it as a vec4 in glsl
	bone_ids_attr.offset = offsetof(SDS::SkinnedVertex, bone_ids);

	VkVertexInputAttributeDescription bone_weights_attr{};
	bone_weights_attr.binding = 0;
	bone_weights_attr.location = 4;
	bone_weights_attr.format = VK_FORMAT_R32G32B32_SFLOAT; // using float as it's more convenient, we can simply refer to it as a vec4 in glsl
	bone_weights_attr.offset = offsetof(SDS::SkinnedVertex, bone_weights);

	return {position_attr, texCoord_attr, normal_attr, bone_ids_attr, bone_weights_attr};
}
