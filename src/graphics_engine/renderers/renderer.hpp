#pragma once

#include "graphics_engine/graphics_engine_base_module.hpp"
#include "graphics_engine/vulkan_wrappers.hpp"
#include "graphics_engine/constants.hpp"
#include "graphics_engine/pipeline/pipeline_id.hpp"
#include "graphics_engine/pipeline/pipeline_types.hpp"

#include <vulkan/vulkan.hpp>


enum class ERendererType
{
	NONE,
	RASTERIZATION,
	RAYTRACING,
	GUI,
	OFFSCREEN_GUI_VIEWPORT,
	SHADOW_MAP,
	QUAD,
};

class GraphicsEngineObject;
class GraphicsEnginePipeline;

// A renderer is simply anything that submits draw commands and fills up a command buffer
// Each renderer can only have ONE renderpass and each renderpass must be unique to the renderer
class Renderer : public GraphicsEngineBaseModule
{
public:
	Renderer(GraphicsEngine& engine);
	~Renderer();

	// generates framebuffers
	virtual void allocate_per_frame_resources(VkImage presentation_image, VkImageView presentation_image_view) = 0;
	virtual void submit_draw_commands(VkCommandBuffer command_buffer, VkImageView presentation_image_view, uint32_t frame_index) = 0;
	virtual constexpr ERendererType get_renderer_type() const = 0;
	virtual VkImageView get_output_image_view(uint32_t frame_idx) = 0;

	virtual VkSampleCountFlagBits get_msaa_sample_count() const { return CSTS::MSAA_SAMPLE_COUNT; }
	virtual VkExtent2D get_extent();
	
	VkRenderPass get_render_pass() { return render_pass; }
	
	virtual void draw_object(VkCommandBuffer command_buffer,
							 uint32_t frame_index,
							 const GraphicsEngineObject& object, 
							 EPipelineModifier pipeline_modifier,
							 EPipelineType primary_pipeline_override = EPipelineType::UNASSIGNED);

protected:
	static constexpr uint32_t get_num_inflight_frames();

protected:
	// A render pass is a general description of steps to draw something on the screen
	//	it's made of at least 1 subpass, which can be executed in parallel (subpasses are mostly used in mobile optimisations)
	//	and also sequentially fed into each other (however this doesnt mean it makes
	//	for a good processor as it can not sample surrounding pixels https://www.saschawillems.de/blog/2018/07/19/vulkan-input-attachments-and-sub-passes/)
	// A render pass when provided a framebuffer will render into said buffer
	// Multiple render passes would be used for post processing effects
	VkRenderPass render_pass = VK_NULL_HANDLE;

	// per frame resources
	std::vector<VkFramebuffer> frame_buffers;
};