#pragma once

#include "pipeline.hpp"
#include "pipeline_modifiers.hpp"


template<typename GraphicsEngineT>
class TexturePipeline : public GraphicsEnginePipeline<GraphicsEngineT>
{
public:
	TexturePipeline(GraphicsEngineT& engine) : GraphicsEnginePipeline<GraphicsEngineT>(engine) {}

	static uint32_t get_vertex_stride() { return sizeof(SDS::TexVertex); }
	static uint32_t get_vertex_pos_offset() { return offsetof(SDS::TexVertex, pos); }

protected:
	virtual std::string_view get_shader_name() const override { return "texture"; }
	virtual std::vector<VkVertexInputBindingDescription> get_binding_descriptions() const override;
	virtual std::vector<VkVertexInputAttributeDescription> get_attribute_descriptions() const override;
};

template<typename GraphicsEngineT>
class ColorPipeline : public GraphicsEnginePipeline<GraphicsEngineT>
{
public:
	ColorPipeline(GraphicsEngineT& engine) : GraphicsEnginePipeline<GraphicsEngineT>(engine) {}

	static uint32_t get_vertex_stride() { return sizeof(SDS::ColorVertex); }
	static uint32_t get_vertex_pos_offset() { return offsetof(SDS::ColorVertex, pos); }

protected:
	virtual std::string_view get_shader_name() const override { return "color"; }
};

template<typename GraphicsEngineT>
class CubemapPipeline : public GraphicsEnginePipeline<GraphicsEngineT>
{
public:
	CubemapPipeline(GraphicsEngineT& engine) : GraphicsEnginePipeline<GraphicsEngineT>(engine) {}

protected:
	virtual std::vector<VkVertexInputBindingDescription> get_binding_descriptions() const override;
	virtual std::vector<VkVertexInputAttributeDescription> get_attribute_descriptions() const override;
	virtual std::string_view get_shader_name() const override { return "cubemap"; }
	virtual VkFrontFace get_front_face() const override { return VkFrontFace::VK_FRONT_FACE_COUNTER_CLOCKWISE; }
	virtual VkPipelineDepthStencilStateCreateInfo get_depth_stencil_create_info() const override;
};

template<typename GraphicsEngineT, Stencileable PrimaryPipelineType>
class StencilPipeline : public GraphicsEnginePipeline<GraphicsEngineT>
{
public:
	StencilPipeline(GraphicsEngineT& engine) : GraphicsEnginePipeline<GraphicsEngineT>(engine) {}

protected:
	virtual std::string_view get_shader_name() const override { return "stencil"; }
	virtual VkPipelineDepthStencilStateCreateInfo get_depth_stencil_create_info() const override;
	virtual std::vector<VkVertexInputBindingDescription> get_binding_descriptions() const override;
	virtual std::vector<VkVertexInputAttributeDescription> get_attribute_descriptions() const override;
};

template<typename GraphicsEngineT, Wireframeable PrimaryPipelineType>
class WireframePipeline : public GraphicsEnginePipeline<GraphicsEngineT>
{
public:
	WireframePipeline(GraphicsEngineT& engine) : GraphicsEnginePipeline<GraphicsEngineT>(engine) {}

protected:
	virtual std::string_view get_shader_name() const override { return "wireframe"; }
	virtual VkPolygonMode get_polygon_mode() const override { return VkPolygonMode::VK_POLYGON_MODE_LINE; }
	virtual std::vector<VkVertexInputBindingDescription> get_binding_descriptions() const override;
	virtual std::vector<VkVertexInputAttributeDescription> get_attribute_descriptions() const override;
};

template<typename GraphicsEngineT>
class RaytracingPipeline : public GraphicsEnginePipeline<GraphicsEngineT>
{
public:
	RaytracingPipeline(GraphicsEngineT& engine) : GraphicsEnginePipeline<GraphicsEngineT>(engine) {}

protected:
	virtual std::string_view get_shader_name() const override { return "simple_raytracer"; }
	virtual void initialise() override;

private:
	std::vector<VkRayTracingShaderGroupCreateInfoKHR> shader_groups;
};

template<typename GraphicsEngineT>
class LightWeightOffscreenPipeline : public GraphicsEnginePipeline<GraphicsEngineT>
{
public:
	LightWeightOffscreenPipeline(GraphicsEngineT& engine) : GraphicsEnginePipeline<GraphicsEngineT>(engine) {}

protected:
	virtual std::string_view get_shader_name() const override { return "color"; }
	virtual VkSampleCountFlagBits get_msaa_sample_count() override { return VkSampleCountFlagBits::VK_SAMPLE_COUNT_1_BIT; }
	virtual VkRenderPass get_render_pass() override;
	virtual VkExtent2D get_extent() override;
	virtual std::vector<VkDescriptorSetLayout> get_expected_dset_layouts() override;
};

template<typename GraphicsEngineT>
class SkinnedPipeline : public GraphicsEnginePipeline<GraphicsEngineT>
{
public:
	SkinnedPipeline(GraphicsEngineT& engine) : GraphicsEnginePipeline<GraphicsEngineT>(engine) {}

	static uint32_t get_vertex_stride() { return sizeof(SDS::SkinnedVertex); }
	static uint32_t get_vertex_pos_offset() { return offsetof(SDS::SkinnedVertex, pos); }

protected:
	virtual std::string_view get_shader_name() const override { return "skinned"; }
	virtual std::vector<VkVertexInputBindingDescription> get_binding_descriptions() const override;
	virtual std::vector<VkVertexInputAttributeDescription> get_attribute_descriptions() const override;
};

template<typename GraphicsEngineT, ShadowMappable PrimaryPipelineType>
class ShadowMapPipeline : public GraphicsEnginePipeline<GraphicsEngineT>
{
public:
	ShadowMapPipeline(GraphicsEngineT& engine) : GraphicsEnginePipeline<GraphicsEngineT>(engine) {}

protected:
	virtual std::string_view get_shader_name() const override { return "shadow_map"; }
	virtual std::vector<VkVertexInputBindingDescription> get_binding_descriptions() const override;
	virtual std::vector<VkVertexInputAttributeDescription> get_attribute_descriptions() const override;
	// virtual VkPipelineDepthStencilStateCreateInfo get_depth_stencil_create_info() const override;
	virtual VkRenderPass get_render_pass() override;
	virtual VkExtent2D get_extent() override;
	virtual VkSampleCountFlagBits get_msaa_sample_count() override;
	virtual std::vector<VkDescriptorSetLayout> get_expected_dset_layouts() override;
};

template<typename GraphicsEngineT>
class QuadPipeline : public GraphicsEnginePipeline<GraphicsEngineT>
{
public:
	QuadPipeline(GraphicsEngineT& engine) : GraphicsEnginePipeline<GraphicsEngineT>(engine) {}

protected:
	virtual std::string_view get_shader_name() const override { return "quad"; }
	virtual std::vector<VkVertexInputBindingDescription> get_binding_descriptions() const override { return {}; }
	virtual std::vector<VkVertexInputAttributeDescription> get_attribute_descriptions() const override { return {}; }
	virtual VkRenderPass get_render_pass() override;
	virtual VkExtent2D get_extent() override;
	virtual VkSampleCountFlagBits get_msaa_sample_count() override;
	virtual std::vector<VkDescriptorSetLayout> get_expected_dset_layouts() override;
	virtual std::vector<VkPushConstantRange> get_push_constant_ranges() const override;
};