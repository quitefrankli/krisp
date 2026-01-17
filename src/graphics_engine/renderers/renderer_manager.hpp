#pragma once

#include "renderer.hpp"
#include "graphics_engine/vulkan_wrappers.hpp"

#include <map>
#include <memory>


class RendererManager : public GraphicsEngineBaseModule
{
public:
	RendererManager(GraphicsEngine& engine);
	Renderer& get_renderer(ERendererType type)
	{
		auto it = renderers.find(type);
		if (it == renderers.end())
		{
			throw std::runtime_error("RendererManager: Renderer not found");
		}

		return *it->second;
	}

	std::map<ERendererType, std::unique_ptr<Renderer>>& get_renderers() { return renderers; }

	// links the inputs and outputs of specific renderers
	void linkup_renderers();
	void pipe_output_to_quad_renderer(ERendererType src_renderer);

private:
	std::map<ERendererType, std::unique_ptr<Renderer>> renderers;
};