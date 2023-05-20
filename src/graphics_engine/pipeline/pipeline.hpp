#pragma once

#include "graphics_engine/graphics_engine_base_module.hpp"
#include "pipeline_types.hpp"
#include "pipeline_id.hpp"

#include <vulkan/vulkan.hpp>

#include <string_view>


template<typename GraphicsEngineT>
class GraphicsEnginePipelineManager;

template<typename GraphicsEngineT>
class GraphicsEnginePipeline : public GraphicsEngineBaseModule<GraphicsEngineT>
{
public:
	~GraphicsEnginePipeline();

	VkPipeline graphics_pipeline = VK_NULL_HANDLE;
	VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;

protected:
	GraphicsEnginePipeline(GraphicsEngineT& engine);
	virtual void initialise(); // should be called after construction
	
	virtual std::string_view get_shader_name() const = 0;
	// face that is not culled
	virtual VkFrontFace get_front_face() const { return VkFrontFace::VK_FRONT_FACE_CLOCKWISE; }
	virtual VkPolygonMode get_polygon_mode() const { return VkPolygonMode::VK_POLYGON_MODE_FILL; }
	virtual VkPipelineDepthStencilStateCreateInfo get_depth_stencil_create_info() const;

	VkShaderModule create_shader_module(const std::string_view filename);
	virtual VkVertexInputBindingDescription get_binding_description() const;
	virtual std::vector<VkVertexInputAttributeDescription> get_attribute_descriptions() const;

	virtual VkExtent2D get_extent();
	virtual VkSampleCountFlagBits get_msaa_sample_counts();
	virtual VkRenderPass get_render_pass();

protected:
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_graphics_engine;
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_logical_device;
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_rsrc_mgr;

private:
	friend GraphicsEnginePipelineManager<GraphicsEngineT>;
};