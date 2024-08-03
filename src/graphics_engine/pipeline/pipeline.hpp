#pragma once

#include "graphics_engine/graphics_engine_base_module.hpp"
#include "renderable/render_types.hpp"
#include "pipeline_id.hpp"

#include <vulkan/vulkan.hpp>

#include <string_view>


class GraphicsEnginePipelineManager;

class GraphicsEnginePipeline : public GraphicsEngineBaseModule
{
public:
	~GraphicsEnginePipeline();

	VkPipeline graphics_pipeline = VK_NULL_HANDLE;
	VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;

protected:
	GraphicsEnginePipeline(GraphicsEngine& engine);
	virtual void initialise(); // should be called after construction
	
	virtual std::string_view get_shader_name() const = 0;
	// face that is not culled
	virtual VkFrontFace get_front_face() const { return VkFrontFace::VK_FRONT_FACE_CLOCKWISE; }
	virtual VkPolygonMode get_polygon_mode() const { return VkPolygonMode::VK_POLYGON_MODE_FILL; }
	virtual VkPipelineDepthStencilStateCreateInfo get_depth_stencil_create_info() const;
	virtual std::vector<VkDescriptorSetLayout> get_expected_dset_layouts();

	VkShaderModule create_shader_module(const std::string_view filename);
	virtual std::vector<VkVertexInputBindingDescription> get_binding_descriptions() const;
	virtual std::vector<VkVertexInputAttributeDescription> get_attribute_descriptions() const;

	virtual VkExtent2D get_extent();
	virtual VkSampleCountFlagBits get_msaa_sample_count();
	virtual VkRenderPass get_render_pass();

	virtual VkCullModeFlags get_cull_mode() const { return VkCullModeFlagBits::VK_CULL_MODE_BACK_BIT; }
	virtual std::vector<VkPushConstantRange> get_push_constant_ranges() const { return {}; }

	virtual void mod_rasterization_state_info(VkPipelineRasterizationStateCreateInfo& rasterization_state_info) const {}

private:
	friend GraphicsEnginePipelineManager;
};