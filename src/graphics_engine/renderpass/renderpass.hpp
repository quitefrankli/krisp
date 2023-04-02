#pragma once

#include "graphics_engine/graphics_engine_base_module.hpp"

#include <vulkan/vulkan.hpp>


// template<typename GraphicsEngineT>
// class RenderPass : public GraphicsEngineBaseModule<GraphicsEngineT>
// {
// public:
// 	RenderPass(GraphicsEngineT& engine);
// 	RenderPass(const RenderPass&) = delete;
// 	RenderPass(RenderPass&&) = delete;
// 	virtual ~RenderPass();

// protected:
// 	VkImage image;
// 	VkImageView image_view;

// private:
// };