#pragma once

#include "graphics_engine_base_module.hpp"
#include "objects.hpp"

#include <vulkan/vulkan.hpp>


class Object;
class GraphicsEngine;
class GraphicsEngineTexture;

class GraphicsEngineObject : public GraphicsEngineBaseModule
{
public:
	GraphicsEngineObject(GraphicsEngine& engine);
	GraphicsEngineObject(GraphicsEngine& engine, std::shared_ptr<Object>&& game_engine_object);
	GraphicsEngineObject(GraphicsEngineObject&& object) noexcept;

	~GraphicsEngineObject();

	std::shared_ptr<Object> object;

	//
	// I realised i messed up big time, these resources should be per swapchain image per object
	// (perhaps vertex can be just per object unless we decide to add dynamic meshes)
	// however uniform buffer should dedfinently be per swapchain image per object
	//

	VkBuffer vertex_buffer;
	VkDeviceMemory vertex_buffer_memory;

	// as opposed to vertex_buffers we expect to change uniform buffer every frame
	VkBuffer uniform_buffer;
	VkDeviceMemory uniform_buffer_memory;

	std::vector<std::vector<Vertex>>& get_vertex_sets();
	VkImageView& get_texture_image_view();
	VkSampler& get_texture_sampler();
	
	GraphicsEngineTexture* texture = nullptr;
private:
	// we need this here so that when we perform a std::move we don't destroy the old memory
	bool require_cleanup = true;
};