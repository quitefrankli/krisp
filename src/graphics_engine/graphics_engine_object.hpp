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
	std::optional<SkeletonID> get_skeleton_id() const;

	void mark_for_delete() { marked_for_delete = true; }
	bool is_marked_for_delete() const { return marked_for_delete; }

	VkDescriptorSet get_renderable_frame_dset(uint8_t frame_idx, uint32_t renderable_idx) const
	{
		return renderable_frame_dsets.at(renderable_idx).at(frame_idx);
	}
	void set_renderable_frame_dsets(std::vector<std::vector<VkDescriptorSet>> dsets)
	{
		renderable_frame_dsets = std::move(dsets);
	}

	const std::vector<VkDescriptorSet>& get_renderable_dsets() const { return renderable_dsets; }
	void set_renderable_dsets(const std::vector<VkDescriptorSet>& dsets) { renderable_dsets = dsets; }

private:
	bool marked_for_delete = false;
	std::vector<VkDescriptorSet> renderable_dsets; // i.e. mesh data
	std::vector<std::vector<VkDescriptorSet>> renderable_frame_dsets;
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
