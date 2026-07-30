#pragma once

#include "graphics_engine_base_module.hpp"
#include "pipeline/pipeline.hpp"
#include "identifications.hpp"
#include "render_frame.hpp"

#include <vulkan/vulkan.hpp>


class GraphicsEngineTexture;

class GraphicsEngineObject : public GraphicsEngineBaseModule
{
public:
	GraphicsEngineObject(
		GraphicsEngine& engine,
		RenderObjectDefinitionPtr definition);
	~GraphicsEngineObject();

	GraphicsEngineObject() = delete;
	GraphicsEngineObject(const GraphicsEngineObject&) = delete;
	GraphicsEngineObject(GraphicsEngineObject&&) = delete;
	GraphicsEngineObject& operator=(const GraphicsEngineObject&) = delete;
	GraphicsEngineObject& operator=(GraphicsEngineObject&&) = delete;

	ObjectID get_id() const;
	bool get_visibility() const;
	const glm::mat4& get_model_transform() const;

	const std::vector<RenderableDefinition>& get_renderables() const;
	std::optional<SkeletonID> get_skeleton_id() const;
	RenderDefinitionVersion get_definition_version() const { return definition->version; }

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
	RenderObjectDefinitionPtr definition;
	std::vector<VkDescriptorSet> renderable_dsets; // material and texture bindings
	std::vector<std::vector<VkDescriptorSet>> renderable_frame_dsets;
};
