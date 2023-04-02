#pragma once

#include "pipelines.hpp"


template<typename GraphicsEngineT>
inline VkPipelineDepthStencilStateCreateInfo StencilPipeline<GraphicsEngineT>::get_depth_stencil_create_info() const
{
	VkPipelineDepthStencilStateCreateInfo info = GraphicsEnginePipeline<GraphicsEngineT>::get_depth_stencil_create_info();
	info.stencilTestEnable = VK_TRUE;
	// render all objects twice
	// on first render always succeed and write 1 to stencil buffer
	// on second render only render if stencil buffer DOES NOT have 1 and don't write to buffer
	info.front.compareOp = VkCompareOp::VK_COMPARE_OP_NOT_EQUAL;
	info.front.passOp = VkStencilOp::VK_STENCIL_OP_KEEP;
	info.front.failOp = VkStencilOp::VK_STENCIL_OP_KEEP;
	info.front.depthFailOp = VkStencilOp::VK_STENCIL_OP_KEEP;

	return info;
}
