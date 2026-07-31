#pragma once

#include "graphics_engine_base_module.hpp"
#include "identifications.hpp"
#include "render_frame.hpp"

#include <vulkan/vulkan.hpp>

#include <vector>


struct GraphicsRenderableResources
{
	RenderableID id;
	uint32_t frame_allocation_count = 0;
	VkDescriptorSet dset = VK_NULL_HANDLE;
	std::vector<VkDescriptorSet> frame_dsets;
};

class GraphicsRenderable : public GraphicsEngineBaseModule
{
public:
	GraphicsRenderable(
		GraphicsEngine& engine,
		RenderableDefinitionPtr definition);
	~GraphicsRenderable();

	GraphicsRenderable() = delete;
	GraphicsRenderable(const GraphicsRenderable&) = delete;
	GraphicsRenderable(GraphicsRenderable&&) = delete;
	GraphicsRenderable& operator=(const GraphicsRenderable&) = delete;
	GraphicsRenderable& operator=(GraphicsRenderable&&) = delete;

	RenderableID get_id() const { return definition->id; }
	uint32_t get_frame_allocation_count() const { return frame_allocation_count; }
	void record_frame_allocation() { ++frame_allocation_count; }
	GraphicsRenderableResources take_graphics_resources() noexcept;
	std::optional<ObjectID> get_object_id() const { return definition->object_id; }
	std::optional<SkeletonID> get_skeleton_id() const;
	const RenderableDefinition& get_definition() const { return *definition; }
	bool get_visibility() const;
	const glm::mat4& get_model_transform() const;

	VkDescriptorSet get_frame_dset(uint8_t frame_index) const
	{
		return frame_dsets.at(frame_index);
	}
	void set_frame_dsets(std::vector<VkDescriptorSet> dsets) { frame_dsets = std::move(dsets); }
	VkDescriptorSet get_dset() const { return dset; }
	void set_dset(VkDescriptorSet value) { dset = value; }

private:
	RenderableDefinitionPtr definition;
	uint32_t frame_allocation_count;
	VkDescriptorSet dset = VK_NULL_HANDLE;
	std::vector<VkDescriptorSet> frame_dsets;
};
