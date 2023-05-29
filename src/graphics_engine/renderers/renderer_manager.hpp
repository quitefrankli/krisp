#pragma once

#include "renderer.hpp"
#include "graphics_engine/vulkan_wrappers.hpp"

#include <map>
#include <memory>


template<typename GraphicsEngineT>
class RendererManager : public GraphicsEngineBaseModule<GraphicsEngineT>
{
public:
	RendererManager(GraphicsEngineT& engine);
	Renderer<GraphicsEngineT>& get_renderer(ERendererType type)
	{
		auto it = renderers.find(type);
		if (it == renderers.end())
		{
			throw std::runtime_error("RendererManager: Renderer not found");
		}

		return *it->second;
	}

	std::vector<Renderer<GraphicsEngineT>*> get_renderers()
	{
		std::vector<Renderer<GraphicsEngineT>*> vec;
		for (auto& [type, renderer] : renderers)
		{
			vec.push_back(renderer.get());
		}

		return vec;
	}

	// links the inputs and outputs of specific renderers
	void linkup_renderers();
	void pipe_output_to_quad_renderer(ERendererType src_renderer);

private:
	std::map<ERendererType, std::unique_ptr<Renderer<GraphicsEngineT>>> renderers;

	using GraphicsEngineBaseModule<GraphicsEngineT>::get_graphics_engine;
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_rsrc_mgr;
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_logical_device;
};