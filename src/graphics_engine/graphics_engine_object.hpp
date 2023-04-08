#pragma once

#include "graphics_engine_base_module.hpp"
#include "pipeline/pipeline.hpp"

#include <vulkan/vulkan.hpp>


class Object;
class Shape;
template<typename GraphicsEngineT>
class GraphicsEngineTexture;

template<typename GraphicsEngineT>
class GraphicsEngineObject : public GraphicsEngineBaseModule<GraphicsEngineT>
{
public:
	GraphicsEngineObject(GraphicsEngineT& engine, const Object& object);
	virtual ~GraphicsEngineObject();

	GraphicsEngineObject() = delete;
	GraphicsEngineObject(const GraphicsEngineObject&) = delete;
	GraphicsEngineObject(GraphicsEngineObject&&) = delete;
	GraphicsEngineObject& operator=(const GraphicsEngineObject&) = delete;
	GraphicsEngineObject& operator=(GraphicsEngineObject&&) = delete;

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
	uint32_t get_num_primitives() const;
	// replaces the old "get_vertex_sets" function
	const std::vector<Shape>& get_shapes() const;

	void mark_for_delete() { marked_for_delete = true; }
	bool is_marked_for_delete() const { return marked_for_delete; }

	// I think we need a couple more different derived GraphcisEngineObjects, i.e. light_source/textured/colored
	const std::vector<GraphicsEngineTexture<GraphicsEngineT>*>& get_textures() const { return textures; }

	// doesn't need to be cleaned up, as descriptor pool will automatically clean it up
	std::vector<VkDescriptorSet> descriptor_sets;

	EPipelineType get_render_type() const { return type; }

	const EPipelineType type;

private:
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_graphics_engine;
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_logical_device;
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_physical_device;
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_instance;
	using GraphicsEngineBaseModule<GraphicsEngineT>::create_buffer;
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_num_swapchain_frames;
	
	bool marked_for_delete = false;

	std::vector<GraphicsEngineTexture<GraphicsEngineT>*> textures;
};

// this object derivation CAN be destroyed while graphics engine is running
template<typename GraphicsEngineT>
class GraphicsEngineObjectPtr : public GraphicsEngineObject<GraphicsEngineT>
{
public:
	GraphicsEngineObjectPtr(GraphicsEngineT& engine, std::shared_ptr<Object>&& game_engine_object);

	const Object& get_game_object() const override;

private:
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_graphics_engine;
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_logical_device;
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_physical_device;
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_instance;
	using GraphicsEngineBaseModule<GraphicsEngineT>::create_buffer;
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_num_swapchain_frames;
	
	std::shared_ptr<Object> object;
};

// this object derivation CANNOT be destroyed while graphics engine is running
template<typename GraphicsEngineT>
class GraphicsEngineObjectRef : public GraphicsEngineObject<GraphicsEngineT>
{
public:
	GraphicsEngineObjectRef(GraphicsEngineT& engine, Object& game_engine_object);
	
	const Object& get_game_object() const override;

private:
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_graphics_engine;
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_logical_device;
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_physical_device;
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_instance;
	using GraphicsEngineBaseModule<GraphicsEngineT>::create_buffer;
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_num_swapchain_frames;
		
	Object& object;
};
