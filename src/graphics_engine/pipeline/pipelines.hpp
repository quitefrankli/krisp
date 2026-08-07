#pragma once

#include "pipeline.hpp"
#include "pipeline_modifiers.hpp"


class TexturePipeline : public GraphicsEnginePipeline
{
public:
	TexturePipeline(GraphicsEngine& engine) : GraphicsEnginePipeline(engine) {}

	static uint32_t get_vertex_stride() { return sizeof(SDS::TexVertex); }
	static uint32_t get_vertex_pos_offset() { return offsetof(SDS::TexVertex, pos); }
	static uint32_t get_vertex_normal_offset() { return offsetof(SDS::TexVertex, normal); }

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
	static uint32_t get_vertex_normal_offset() { return offsetof(SDS::ColorVertex, normal); }

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

// For ray tracing:
// class RaytracingPipeline : public GraphicsEnginePipeline
// {
// public:
// 	RaytracingPipeline(GraphicsEngine& engine) : GraphicsEnginePipeline(engine) {}
//
// protected:
// 	virtual std::string_view get_shader_name() const override { return "simple_raytracer"; }
// 	virtual void initialise() override;
//
// private:
// 	std::vector<VkRayTracingShaderGroupCreateInfoKHR> shader_groups;
// };

class SkinnedPipeline : public GraphicsEnginePipeline
{
public:
	SkinnedPipeline(GraphicsEngine& engine) : GraphicsEnginePipeline(engine) {}

	static uint32_t get_vertex_stride() { return sizeof(SDS::SkinnedVertex); }
	static uint32_t get_vertex_pos_offset() { return offsetof(SDS::SkinnedVertex, pos); }
	static uint32_t get_vertex_normal_offset() { return offsetof(SDS::SkinnedVertex, normal); }
	static std::vector<VkVertexInputBindingDescription> get_binding_descriptions_();
	static std::vector<VkVertexInputAttributeDescription> get_attribute_descriptions_();
	static std::vector<VkVertexInputAttributeDescription> get_skinning_attribute_descriptions_();

protected:
	virtual std::string_view get_shader_name() const override { return "skinned"; }
	virtual std::vector<VkVertexInputBindingDescription> get_binding_descriptions() const override;
	virtual std::vector<VkVertexInputAttributeDescription> get_attribute_descriptions() const override;
};

class SkinnedColorPipeline : public SkinnedPipeline
{
public:
	SkinnedColorPipeline(GraphicsEngine& engine) : SkinnedPipeline(engine) {}

protected:
	virtual std::string_view get_shader_name() const override { return "skinned_color"; }
	virtual std::vector<VkVertexInputAttributeDescription> get_attribute_descriptions() const override;
};

class UnlitTexturePipeline : public TexturePipeline
{
public:
	UnlitTexturePipeline(GraphicsEngine& engine) : TexturePipeline(engine) {}

protected:
	virtual std::string_view get_shader_name() const override { return "unlit_texture"; }
	virtual std::vector<VkVertexInputAttributeDescription> get_attribute_descriptions() const override;
};

class UnlitColorPipeline : public ColorPipeline
{
public:
	UnlitColorPipeline(GraphicsEngine& engine) : ColorPipeline(engine) {}

protected:
	virtual std::string_view get_shader_name() const override { return "unlit_color"; }
	virtual std::vector<VkVertexInputAttributeDescription> get_attribute_descriptions() const override;
};

class UnlitSkinnedPipeline : public SkinnedPipeline
{
public:
	UnlitSkinnedPipeline(GraphicsEngine& engine) : SkinnedPipeline(engine) {}

protected:
	virtual std::string_view get_shader_name() const override { return "unlit_skinned"; }
	virtual std::vector<VkVertexInputAttributeDescription> get_attribute_descriptions() const override;
};

class UnlitSkinnedColorPipeline : public SkinnedColorPipeline
{
public:
	UnlitSkinnedColorPipeline(GraphicsEngine& engine) : SkinnedColorPipeline(engine) {}

protected:
	virtual std::string_view get_shader_name() const override { return "unlit_skinned_color"; }
	virtual std::vector<VkVertexInputAttributeDescription> get_attribute_descriptions() const override;
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

template<>
class WireframePipeline<SkinnedPipeline> : public GraphicsEnginePipeline
{
public:
	WireframePipeline(GraphicsEngine& engine) : GraphicsEnginePipeline(engine) {}

protected:
	virtual std::string_view get_shader_name() const override { return "wireframe_skinned"; }
	virtual VkPolygonMode get_polygon_mode() const override { return VkPolygonMode::VK_POLYGON_MODE_LINE; }
	virtual std::vector<VkVertexInputBindingDescription> get_binding_descriptions() const override;
	virtual std::vector<VkVertexInputAttributeDescription> get_attribute_descriptions() const override;
};

class ShadowMapBasePipeline : public GraphicsEnginePipeline
{
public:
	ShadowMapBasePipeline(GraphicsEngine& engine) : GraphicsEnginePipeline(engine) {}

protected:
	virtual std::string_view get_shader_name() const override;
	virtual VkRenderPass get_render_pass() override;
	virtual VkExtent2D get_extent() override;
	virtual VkSampleCountFlagBits get_msaa_sample_count() override;
	virtual std::vector<VkDescriptorSetLayout> get_expected_dset_layouts() override;
	virtual void mod_rasterization_state_info(VkPipelineRasterizationStateCreateInfo& rasterization_state_info) const override;
};

template<ShadowMappable PrimaryPipelineType>
class ShadowMapPipeline : public ShadowMapBasePipeline
{
public:
	ShadowMapPipeline(GraphicsEngine& engine) : ShadowMapBasePipeline(engine) {}

protected:
	virtual std::vector<VkVertexInputBindingDescription> get_binding_descriptions() const override;
	virtual std::vector<VkVertexInputAttributeDescription> get_attribute_descriptions() const override;
};

template<>
class ShadowMapPipeline<SkinnedPipeline> : public ShadowMapBasePipeline
{
public:
	ShadowMapPipeline(GraphicsEngine& engine) : ShadowMapBasePipeline(engine) {}

protected:
	virtual std::string_view get_shader_name() const override;
	virtual std::vector<VkVertexInputBindingDescription> get_binding_descriptions() const override;
	virtual std::vector<VkVertexInputAttributeDescription> get_attribute_descriptions() const override;
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

class PresentationPipeline : public GraphicsEnginePipeline
{
public:
	PresentationPipeline(GraphicsEngine& engine) : GraphicsEnginePipeline(engine) {}

protected:
	// A vertex-free full-screen triangle samples the resolved HDR scene. Exposure
	// and tone mapping are fragment work; the sRGB attachment performs encoding.
	std::string_view get_shader_name() const override { return "presentation"; }
	std::vector<VkVertexInputBindingDescription> get_binding_descriptions() const override { return {}; }
	std::vector<VkVertexInputAttributeDescription> get_attribute_descriptions() const override { return {}; }
	VkRenderPass get_render_pass() override;
	VkExtent2D get_extent() override;
	VkSampleCountFlagBits get_msaa_sample_count() override { return VK_SAMPLE_COUNT_1_BIT; }
	std::vector<VkDescriptorSetLayout> get_expected_dset_layouts() override;
	std::vector<VkPushConstantRange> get_push_constant_ranges() const override;
	VkPipelineDepthStencilStateCreateInfo get_depth_stencil_create_info() const override;
};

template<Stencileable PrimaryPipelineType>
class StencilPipeline : public GraphicsEnginePipeline
{
public:
	StencilPipeline(GraphicsEngine& engine) : GraphicsEnginePipeline(engine) {}

protected:
	virtual std::string_view get_shader_name() const override { return "stencil"; }
	virtual VkPipelineDepthStencilStateCreateInfo get_depth_stencil_create_info() const override;
	virtual VkCullModeFlags get_cull_mode() const override { return VK_CULL_MODE_FRONT_BIT; }
	virtual std::vector<VkVertexInputBindingDescription> get_binding_descriptions() const override;
	virtual std::vector<VkVertexInputAttributeDescription> get_attribute_descriptions() const override;
};

template<>
class StencilPipeline<SkinnedPipeline> : public GraphicsEnginePipeline
{
public:
	StencilPipeline(GraphicsEngine& engine) : GraphicsEnginePipeline(engine) {}

protected:
	virtual std::string_view get_shader_name() const override { return "stencil_skinned"; }
	virtual VkPipelineDepthStencilStateCreateInfo get_depth_stencil_create_info() const override;
	virtual VkCullModeFlags get_cull_mode() const override { return VK_CULL_MODE_FRONT_BIT; }
	virtual std::vector<VkVertexInputBindingDescription> get_binding_descriptions() const override;
	virtual std::vector<VkVertexInputAttributeDescription> get_attribute_descriptions() const override;
};

template<Stencileable PrimaryPipelineType>
class PostStencilPipeline : public PrimaryPipelineType
{
public:
	PostStencilPipeline(GraphicsEngine& engine) : PrimaryPipelineType(engine) {}

protected:
	virtual VkPipelineDepthStencilStateCreateInfo get_depth_stencil_create_info() const override;
};

class ParticlePipeline : public GraphicsEnginePipeline
{
public:
	ParticlePipeline(GraphicsEngine& engine) : GraphicsEnginePipeline(engine) {}

	static uint32_t get_vertex_stride() { return sizeof(float) * 4; } // 2 floats for position + 2 floats for texcoord
	static uint32_t get_instance_stride() { return sizeof(SDS::ParticleInstanceData); }

protected:
	virtual std::string_view get_shader_name() const override { return "particle"; }
	virtual std::vector<VkVertexInputBindingDescription> get_binding_descriptions() const override;
	virtual std::vector<VkVertexInputAttributeDescription> get_attribute_descriptions() const override;
	virtual VkCullModeFlags get_cull_mode() const override { return VK_CULL_MODE_NONE; } // No culling for particles
	virtual VkPipelineDepthStencilStateCreateInfo get_depth_stencil_create_info() const override;
	virtual VkBlendFactor get_src_blend_factor() const { return VK_BLEND_FACTOR_SRC_ALPHA; }
	virtual VkBlendFactor get_dst_blend_factor() const { return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA; }
	virtual void mod_color_blend_attachment(VkPipelineColorBlendAttachmentState& color_blend_attachment) const;
};
