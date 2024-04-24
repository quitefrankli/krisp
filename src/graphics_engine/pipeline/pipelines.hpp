#pragma once

#include "pipeline.hpp"
#include "pipeline_modifiers.hpp"


class TexturePipeline : public GraphicsEnginePipeline
{
public:
	TexturePipeline(GraphicsEngine& engine) : GraphicsEnginePipeline(engine) {}

	static uint32_t get_vertex_stride() { return sizeof(SDS::TexVertex); }
	static uint32_t get_vertex_pos_offset() { return offsetof(SDS::TexVertex, pos); }

protected:
	virtual std::string_view get_shader_name() const override { return "texture"; }
	virtual std::vector<VkVertexInputBindingDescription> get_binding_descriptions() const override;
	virtual std::vector<VkVertexInputAttributeDescription> get_attribute_descriptions() const override;
};

class ColorPipeline : public GraphicsEnginePipeline
{
public:
	ColorPipeline(GraphicsEngine& engine) : GraphicsEnginePipeline(engine) {}

	static uint32_t get_vertex_stride() { return sizeof(SDS::ColorVertex); }
	static uint32_t get_vertex_pos_offset() { return offsetof(SDS::ColorVertex, pos); }

protected:
	virtual std::string_view get_shader_name() const override { return "color"; }
};

class CubemapPipeline : public GraphicsEnginePipeline
{
public:
	CubemapPipeline(GraphicsEngine& engine) : GraphicsEnginePipeline(engine) {}

protected:
	virtual std::vector<VkVertexInputBindingDescription> get_binding_descriptions() const override;
	virtual std::vector<VkVertexInputAttributeDescription> get_attribute_descriptions() const override;
	virtual std::string_view get_shader_name() const override { return "cubemap"; }
	virtual VkFrontFace get_front_face() const override { return VkFrontFace::VK_FRONT_FACE_COUNTER_CLOCKWISE; }
	virtual VkPipelineDepthStencilStateCreateInfo get_depth_stencil_create_info() const override;
};

template<Stencileable PrimaryPipelineType>
class StencilPipeline : public GraphicsEnginePipeline
{
public:
	StencilPipeline(GraphicsEngine& engine) : GraphicsEnginePipeline(engine) {}

protected:
	virtual std::string_view get_shader_name() const override { return "stencil"; }
	virtual VkPipelineDepthStencilStateCreateInfo get_depth_stencil_create_info() const override;
	virtual std::vector<VkVertexInputBindingDescription> get_binding_descriptions() const override;
	virtual std::vector<VkVertexInputAttributeDescription> get_attribute_descriptions() const override;
};

class PostStencilColorPipeline : public ColorPipeline
{
public:
	PostStencilColorPipeline(GraphicsEngine& engine) : ColorPipeline(engine) {}

protected:
	virtual VkPipelineDepthStencilStateCreateInfo get_depth_stencil_create_info() const override;
};

class PostStencilTexturePipeline : public TexturePipeline
{
public:
	PostStencilTexturePipeline(GraphicsEngine& engine) : TexturePipeline(engine) {}

protected:
	virtual VkPipelineDepthStencilStateCreateInfo get_depth_stencil_create_info() const override;
};

template<Wireframeable PrimaryPipelineType>
class WireframePipeline : public GraphicsEnginePipeline
{
public:
	WireframePipeline(GraphicsEngine& engine) : GraphicsEnginePipeline(engine) {}

protected:
	virtual std::string_view get_shader_name() const override { return "wireframe"; }
	virtual VkPolygonMode get_polygon_mode() const override { return VkPolygonMode::VK_POLYGON_MODE_LINE; }
	virtual std::vector<VkVertexInputBindingDescription> get_binding_descriptions() const override;
	virtual std::vector<VkVertexInputAttributeDescription> get_attribute_descriptions() const override;
};

class RaytracingPipeline : public GraphicsEnginePipeline
{
public:
	RaytracingPipeline(GraphicsEngine& engine) : GraphicsEnginePipeline(engine) {}

protected:
	virtual std::string_view get_shader_name() const override { return "simple_raytracer"; }
	virtual void initialise() override;

private:
	std::vector<VkRayTracingShaderGroupCreateInfoKHR> shader_groups;
};

class LightWeightOffscreenPipeline : public GraphicsEnginePipeline
{
public:
	LightWeightOffscreenPipeline(GraphicsEngine& engine) : GraphicsEnginePipeline(engine) {}

protected:
	virtual std::string_view get_shader_name() const override { return "color"; }
	virtual VkSampleCountFlagBits get_msaa_sample_count() override { return VkSampleCountFlagBits::VK_SAMPLE_COUNT_1_BIT; }
	virtual VkRenderPass get_render_pass() override;
	virtual VkExtent2D get_extent() override;
};

class SkinnedPipeline : public GraphicsEnginePipeline
{
public:
	SkinnedPipeline(GraphicsEngine& engine) : GraphicsEnginePipeline(engine) {}

	static uint32_t get_vertex_stride() { return sizeof(SDS::SkinnedVertex); }
	static uint32_t get_vertex_pos_offset() { return offsetof(SDS::SkinnedVertex, pos); }

protected:
	virtual std::string_view get_shader_name() const override { return "skinned"; }
	virtual std::vector<VkVertexInputBindingDescription> get_binding_descriptions() const override;
	virtual std::vector<VkVertexInputAttributeDescription> get_attribute_descriptions() const override;
};

template<ShadowMappable PrimaryPipelineType>
class ShadowMapPipeline : public GraphicsEnginePipeline
{
public:
	ShadowMapPipeline(GraphicsEngine& engine) : GraphicsEnginePipeline(engine) {}

protected:
	virtual std::string_view get_shader_name() const override { return "shadow_map"; }
	virtual std::vector<VkVertexInputBindingDescription> get_binding_descriptions() const override;
	virtual std::vector<VkVertexInputAttributeDescription> get_attribute_descriptions() const override;
	// virtual VkPipelineDepthStencilStateCreateInfo get_depth_stencil_create_info() const override;
	virtual VkRenderPass get_render_pass() override;
	virtual VkExtent2D get_extent() override;
	virtual VkSampleCountFlagBits get_msaa_sample_count() override;
	virtual std::vector<VkDescriptorSetLayout> get_expected_dset_layouts() override;
	virtual void mod_rasterization_state_info(VkPipelineRasterizationStateCreateInfo& rasterization_state_info) const override;
};

class QuadPipeline : public GraphicsEnginePipeline
{
public:
	QuadPipeline(GraphicsEngine& engine) : GraphicsEnginePipeline(engine) {}

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