#pragma once

#include "graphics_engine_base_module.hpp"
#include "pipeline/pipeline.hpp"
#include "identifications.hpp"
#include "renderable/renderable.hpp"

#include <vulkan/vulkan.hpp>


class Object;
class GraphicsEngineTexture;

class GraphicsEngineObject : public GraphicsEngineBaseModule
{
public:
	GraphicsEngineObject(GraphicsEngine& engine, const Object& object);
	virtual ~GraphicsEngineObject();

	GraphicsEngineObject() = delete;
	GraphicsEngineObject(const GraphicsEngineObject&) = delete;
	GraphicsEngineObject(GraphicsEngineObject&&) = delete;
	GraphicsEngineObject& operator=(const GraphicsEngineObject&) = delete;
	GraphicsEngineObject& operator=(GraphicsEngineObject&&) = delete;

	const virtual Object& get_game_object() const = 0;

	ObjectID get_id() const;
	bool get_visibility() const;

	const std::vector<Renderable>& get_renderables() const;

	void mark_for_delete() { marked_for_delete = true; }
	bool is_marked_for_delete() const { return marked_for_delete; }

	VkDescriptorSet get_obj_dset(uint8_t frame_idx) const { return per_frame_object_dsets[frame_idx]; }
	void set_obj_dsets(const std::vector<VkDescriptorSet>& dsets) { per_frame_object_dsets = dsets; }

	const std::vector<VkDescriptorSet>& get_renderable_dsets() const { return renderable_dsets; }
	void set_renderable_dsets(const std::vector<VkDescriptorSet>& dsets) { renderable_dsets = dsets; }

private:
	bool marked_for_delete = false;
	std::vector<VkDescriptorSet> renderable_dsets; // i.e. mesh data
	std::vector<VkDescriptorSet> per_frame_object_dsets; // i.e. uniform buffer
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
