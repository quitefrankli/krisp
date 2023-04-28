#pragma once

#include "graphics_engine_base_module.hpp"
#include "pipeline/pipeline.hpp"
#include "graphics_materials.hpp"

#include <vulkan/vulkan.hpp>


class Object;
class Shape;
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
	// however uniform buffer should definently be per swapchain image per object
	//

	const virtual Object& get_game_object() const = 0;

	uint32_t get_num_unique_vertices() const;
	uint32_t get_num_vertex_indices() const;
	uint32_t get_num_primitives() const;
	// replaces the old "get_vertex_sets" function
	const std::vector<ShapePtr>& get_shapes() const;

	void mark_for_delete() { marked_for_delete = true; }
	bool is_marked_for_delete() const { return marked_for_delete; }

	const std::vector<GraphicsMaterial>& get_materials() const { return materials; }

	// doesn't need to be cleaned up, as descriptor pool will automatically clean it up
	std::vector<VkDescriptorSet> descriptor_sets;

	// TODO: come up with a different enum class (not pipeline or renderer type) that expresses an object's
	// rendering 'style'. This is because an object can be rendered with different pipeline/renderer than
	// the associated 'style'.
	EPipelineType get_render_type() const { return type; }

	const EPipelineType type;

private:
	bool marked_for_delete = false;

	std::vector<GraphicsMaterial> materials;
};

// this object derivation CAN be destroyed while graphics engine is running
template<typename GraphicsEngineT>
class GraphicsEngineObjectPtr : public GraphicsEngineObject<GraphicsEngineT>
{
public:
	GraphicsEngineObjectPtr(GraphicsEngineT& engine, std::shared_ptr<Object>&& game_engine_object);

	const Object& get_game_object() const override;

private:
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
	Object& object;
};
