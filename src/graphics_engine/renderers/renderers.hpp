#pragma once

#include "renderer.hpp"


template<typename GraphicsEngineT>
class RasterizationRenderer : public Renderer<GraphicsEngineT>
{
public:
	RasterizationRenderer(GraphicsEngineT& engine);
	~RasterizationRenderer();

	virtual void allocate_inflight_frame_resources(VkImage presentation_image) override;
	virtual void submit_draw_commands(VkCommandBuffer command_buffer, uint32_t frame_index) override;
	virtual constexpr ERendererType get_renderer_type() const override { return ERendererType::RASTERIZATION; }

private:
	static constexpr VkFormat get_image_format() { return VK_FORMAT_B8G8R8A8_SRGB; }
	void create_frame_buffer();
	void create_render_pass();

	std::vector<RenderingAttachment> color_attachments;
	std::vector<RenderingAttachment> depth_attachments;
	std::vector<RenderingAttachment> presentation_attachments; // also responsible for msaa resolve
};

// template<typename GraphicsEngineT>
// class RaytracingRenderer : public Renderer<GraphicsEngineT>
// {
// public:
// 	RaytracingRenderer(GraphicsEngineT& engine);

// 	virtual void allocate_inflight_frame_resources(VkImage presentation_image) override;
// 	virtual void submit_draw_commands(VkCommandBuffer command_buffer, uint32_t frame_index) override;
// 	virtual constexpr ERendererType get_renderer_type() const override { return ERendererType::RAYTRACING; }

// private:
// 	void create_frame_buffer();
// 	void create_render_pass();

// 	Attachment color_attachment;
// 	Attachment depth_attachment;
// };

template<typename GraphicsEngineT>
class GuiRenderer : public Renderer<GraphicsEngineT>
{
public:
	GuiRenderer(GraphicsEngineT& engine);
	~GuiRenderer();

	virtual void allocate_inflight_frame_resources(VkImage presentation_image) override;
	virtual void submit_draw_commands(VkCommandBuffer command_buffer, uint32_t frame_index) override;
	virtual constexpr ERendererType get_renderer_type() const override { return ERendererType::GUI; }

private:
	static constexpr VkFormat get_image_format() { return VK_FORMAT_B8G8R8A8_SRGB; }
	void create_frame_buffer();
	void create_render_pass();

	std::vector<RenderingAttachment> presentation_attachments; // also responsible for msaa resolve
};