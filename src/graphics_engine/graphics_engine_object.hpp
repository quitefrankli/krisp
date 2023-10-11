#pragma once

#include "graphics_engine_base_module.hpp"
#include "pipeline/pipeline.hpp"
#include "graphics_materials.hpp"
#include "shapes/shape.hpp"
#include "objects/object_id.hpp"

#include <vulkan/vulkan.hpp>


class Object;
class GraphicsEngineTexture;

class GraphicsMesh // aka GraphicsEngineShape
{
public:
	GraphicsMesh(Shape& shape, GraphicsEngineTexture* texture = nullptr);

	~GraphicsMesh()
	{
		if (dset != VK_NULL_HANDLE)
		{
			throw std::runtime_error("GraphicsMesh: warning dset may not have been properly cleared");
		}
	}

	// const GraphicsMaterial& get_material() const { return material; }
	// const GraphicsEngineTexture* get_texture() const { return texture; }

	ShapeID get_id() const { return shape.get_id(); }

	VkDescriptorSet get_dset() const { return dset; }
	void set_dset(VkDescriptorSet dset) { this->dset = dset; }

	uint32_t get_num_vertex_indices() const { return shape.get_num_vertex_indices(); }
	uint32_t get_num_unique_vertices() const { return shape.get_num_unique_vertices(); }
	const std::byte* get_vertices_data() const { return shape.get_vertices_data(); }
	size_t get_vertices_data_size() const { return shape.get_vertices_data_size(); }
	const std::byte* get_indices_data() const { return shape.get_indices_data(); }
	size_t get_indices_data_size() const { return shape.get_indices_data_size(); }

	GraphicsMaterial& get_material() { return material; }
	const GraphicsMaterial& get_material() const { return material; }

private:
	const Shape& shape;
	VkDescriptorSet dset = VK_NULL_HANDLE;
	GraphicsMaterial material;
};

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
	ObjectID get_id() const { return get_game_object().get_id(); }
	bool get_visibility() const { return get_game_object().get_visibility(); }

	std::vector<GraphicsMesh>& get_shapes() { return meshes; }
	const std::vector<GraphicsMesh>& get_shapes() const { return meshes; }

	void mark_for_delete() { marked_for_delete = true; }
	bool is_marked_for_delete() const { return marked_for_delete; }

	// TODO: come up with a different enum class (not pipeline or renderer type) that expresses an object's
	// rendering 'style'. This is because an object can be rendered with different pipeline/renderer than
	// the associated 'style'.
	EPipelineType get_render_type() const { return type; }

	const EPipelineType type;

	std::vector<VkDescriptorSet>& get_dsets() { return dsets; }
	VkDescriptorSet get_dset(uint32_t frame_idx) const { return dsets[frame_idx]; }
	void set_dset(VkDescriptorSet dset, uint32_t frame_idx) { dsets[frame_idx] = dset; }

private:
	bool marked_for_delete = false;
	std::vector<VkDescriptorSet> dsets;
	std::vector<GraphicsMesh> meshes;
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
