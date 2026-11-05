#pragma once

#include "graphics_engine/graphics_engine_base_module.hpp"
#include "graphics_engine/vulkan_wrappers.hpp"
#include "constants.hpp"
#include "graphics_engine/pipeline/pipeline_id.hpp"
#include "renderable/render_types.hpp"
#include "renderable/material.hpp"
#include "render_frame.hpp"

#include <vulkan/vulkan.hpp>

#include <optional>


enum class ERendererType
{
	NONE,
	RASTERIZATION,
	RAYTRACING,
	GUI,
	SHADOW_MAP,
	QUAD,
	PRESENTATION,
	PARTICLE,
};

class GraphicsEnginePipeline;

// A renderer is simply anything that submits draw commands and fills up a command buffer
class Renderer : public GraphicsEngineBaseModule
{
public:
	Renderer(GraphicsEngine& engine);
	~Renderer();

	// Allocates attachments for one swap-chain image.
	virtual void allocate_per_frame_resources(VkImage presentation_image, VkImageView presentation_image_view) = 0;
	virtual void submit_draw_commands(VkCommandBuffer command_buffer, 
									  VkImageView presentation_image_view, 
									  uint32_t frame_index) = 0;
	virtual constexpr ERendererType get_renderer_type() const = 0;
	virtual VkImageView get_output_image_view(uint32_t frame_idx) = 0;

	virtual VkSampleCountFlagBits get_msaa_sample_count() const { return (VkSampleCountFlagBits)CSTS::MSAA_SAMPLE_COUNT; }
	virtual VkExtent2D get_extent();
	
	
	virtual void draw_renderable(VkCommandBuffer command_buffer,
								 const RenderableDefinition& renderable,
								 const VkDescriptorSet& object_dset,
								 const VkDescriptorSet& renderable_dset,
								 EPipelineModifier pipeline_modifier,
								 ERenderType primary_pipeline_override = ERenderType::UNASSIGNED,
								 std::optional<EShadingMode> shading_override = std::nullopt,
								 uint32_t light_index = 0);
	void reset_draw_state();

private:
	VkPipeline bound_pipeline = VK_NULL_HANDLE;
	std::optional<MeshID> bound_mesh;
};
