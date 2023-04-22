#pragma once

#include "graphics_engine/graphics_engine_base_module.hpp"
#include "pipeline_types.hpp"

#include <vulkan/vulkan.hpp>

#include <string_view>


template<typename GraphicsEngineT>
class GraphicsEnginePipeline : public GraphicsEngineBaseModule<GraphicsEngineT>
{
public:
	static std::unique_ptr<GraphicsEnginePipeline> create_pipeline(GraphicsEngineT& engine, EPipelineType type);
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

	virtual VkExtent2D get_extent();
	virtual VkSampleCountFlagBits get_msaa_sample_counts();
	virtual VkRenderPass get_render_pass();

	using GraphicsEngineBaseModule<GraphicsEngineT>::get_graphics_engine;
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_logical_device;
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_physical_device;
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_instance;
	using GraphicsEngineBaseModule<GraphicsEngineT>::create_buffer;
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_num_swapchain_frames;
};