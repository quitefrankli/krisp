#pragma once

#include "graphics_engine_base_module.hpp"
#include "pipeline/pipeline.hpp"

#include <vulkan/vulkan.hpp>


class Object;
class Shape;
class GraphicsEngine;
class GraphicsEngineTexture;

class GraphicsEngineObject : public GraphicsEngineBaseModule
{
public:
	GraphicsEngineObject(GraphicsEngine& engine, const Object& object);

	~GraphicsEngineObject();

	//
	// I realised i messed up big time, these resources should be per swapchain image per object
	// (perhaps vertex can be just per object unless we decide to add dynamic meshes)
	// however uniform buffer should dedfinently be per swapchain image per object
	//

	const virtual Object& get_game_object() const = 0;

	VkBuffer vertex_buffer;
	VkDeviceMemory vertex_buffer_memory;

	VkBuffer index_buffer;
	VkDeviceMemory index_buffer_memory;

	// as opposed to vertex_buffers we expect to change uniform buffer every frame
	VkBuffer uniform_buffer;
	VkDeviceMemory uniform_buffer_memory;

	uint32_t get_num_unique_vertices() const;
	uint32_t get_num_vertex_indices() const;
	VkImageView& get_texture_image_view();
	VkSampler& get_texture_sampler();
	// replaces the old "get_vertex_sets" function
	const std::vector<Shape>& get_shapes() const;

	GraphicsEngineTexture* texture = nullptr;

	void mark_for_delete() { marked_for_delete = true; }
	bool is_marked_for_delete() const { return marked_for_delete; }

	// doesn't need to be cleaned up, as descriptor pool will automatically clean it up
	std::vector<VkDescriptorSet> descriptor_sets;

	const EPipelineType type;

private:
	bool marked_for_delete = false;
};

// this object derivation CAN be destroyed while graphics engine is running
class GraphicsEngineObjectPtr : public GraphicsEngineObject
{
public:
	GraphicsEngineObjectPtr(GraphicsEngine& engine, std::shared_ptr<Object>&& game_engine_object);
	
	const Object& get_game_object() const override;

private:
	std::shared_ptr<Object> object;
};

// this object derivation CANNOT be destroyed while graphics engine is running
class GraphicsEngineObjectRef : public GraphicsEngineObject
{
public:
	GraphicsEngineObjectRef(GraphicsEngine& engine, Object& game_engine_object);
	
	const Object& get_game_object() const override;

private:
	Object& object;
};
