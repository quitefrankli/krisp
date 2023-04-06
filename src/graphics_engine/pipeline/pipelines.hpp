#pragma once

#include "pipeline.hpp"


template<typename GraphicsEngineT>
class TexturePipeline : public GraphicsEnginePipeline<GraphicsEngineT>
{
public:
	TexturePipeline(GraphicsEngineT& engine) : GraphicsEnginePipeline<GraphicsEngineT>(engine) {}

protected:
	virtual std::string_view get_shader_name() const override { return "texture"; }
};

template<typename GraphicsEngineT>
class ColorPipeline : public GraphicsEnginePipeline<GraphicsEngineT>
{
public:
	ColorPipeline(GraphicsEngineT& engine) : GraphicsEnginePipeline<GraphicsEngineT>(engine) {}

protected:
	virtual std::string_view get_shader_name() const override { return "color"; }
};

template<typename GraphicsEngineT>
class ColorNoLightingPipeline : public GraphicsEnginePipeline<GraphicsEngineT>
{
public:
	ColorNoLightingPipeline(GraphicsEngineT& engine) : GraphicsEnginePipeline<GraphicsEngineT>(engine) {}

protected:
	virtual std::string_view get_shader_name() const override { return "color_no_lighting"; }
};

template<typename GraphicsEngineT>
class CubemapPipeline : public GraphicsEnginePipeline<GraphicsEngineT>
{
public:
	CubemapPipeline(GraphicsEngineT& engine) : GraphicsEnginePipeline<GraphicsEngineT>(engine) {}

protected:
	virtual std::string_view get_shader_name() const override { return "cubemap"; }
	virtual VkFrontFace get_front_face() const override { return VkFrontFace::VK_FRONT_FACE_COUNTER_CLOCKWISE; }
	virtual VkPipelineDepthStencilStateCreateInfo get_depth_stencil_create_info() const override;
};

template<typename GraphicsEngineT>
class StencilPipeline : public GraphicsEnginePipeline<GraphicsEngineT>
{
public:
	StencilPipeline(GraphicsEngineT& engine) : GraphicsEnginePipeline<GraphicsEngineT>(engine) {}

protected:
	virtual std::string_view get_shader_name() const override { return "stencil"; }
	virtual VkPipelineDepthStencilStateCreateInfo get_depth_stencil_create_info() const override;
};

template<typename GraphicsEngineT>
class WireframePipeline : public GraphicsEnginePipeline<GraphicsEngineT>
{
public:
	WireframePipeline(GraphicsEngineT& engine) : GraphicsEnginePipeline<GraphicsEngineT>(engine) {}

protected:
	virtual std::string_view get_shader_name() const override { return "wireframe"; }
	virtual VkPolygonMode get_polygon_mode() const override { return VkPolygonMode::VK_POLYGON_MODE_LINE; }
};

template<typename GraphicsEngineT>
class LightSourcePipeline : public GraphicsEnginePipeline<GraphicsEngineT>
{
public:
	LightSourcePipeline(GraphicsEngineT& engine) : GraphicsEnginePipeline<GraphicsEngineT>(engine) {}

protected:
	virtual std::string_view get_shader_name() const override { return "light_source"; }
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