#pragma once

#include "renderer.hpp"


template<typename GraphicsEngineT>
class RasterizationRenderer : public Renderer<GraphicsEngineT>
{
public:
	RasterizationRenderer(GraphicsEngineT& engine);
	~RasterizationRenderer();

	virtual void allocate_per_frame_resources(VkImage presentation_image, VkImageView presentation_image_view) override;
	virtual void submit_draw_commands(VkCommandBuffer command_buffer, VkImageView presentation_image_view, uint32_t frame_index) override;
	virtual constexpr ERendererType get_renderer_type() const override { return ERendererType::RASTERIZATION; }

private:
	static constexpr VkFormat get_image_format() { return VK_FORMAT_B8G8R8A8_SRGB; }
	void create_render_pass();

	std::vector<RenderingAttachment> color_attachments;
	std::vector<RenderingAttachment> depth_attachments;
	// presentation attachment which is also responsible for msaa resolve, is owned by the swapchain
};

template<typename GraphicsEngineT>
class GuiRenderer : public Renderer<GraphicsEngineT>
{
public:
	GuiRenderer(GraphicsEngineT& engine);
	~GuiRenderer();

	virtual void allocate_per_frame_resources(VkImage presentation_image, VkImageView presentation_image_view) override;
	virtual void submit_draw_commands(VkCommandBuffer command_buffer, VkImageView presentation_image_view, uint32_t frame_index) override;
	virtual constexpr ERendererType get_renderer_type() const override { return ERendererType::GUI; }

private:
	static constexpr VkFormat get_image_format() { return VK_FORMAT_B8G8R8A8_SRGB; }
	void create_render_pass();
};

template<typename GraphicsEngineT>
class RaytracingRenderer : public Renderer<GraphicsEngineT>
{
public:
	RaytracingRenderer(GraphicsEngineT& engine);
	~RaytracingRenderer();

	virtual void allocate_per_frame_resources(VkImage presentation_image, VkImageView presentation_image_view) override;
	virtual void submit_draw_commands(VkCommandBuffer command_buffer, VkImageView presentation_image_view, uint32_t frame_index) override;
	virtual constexpr ERendererType get_renderer_type() const override { return ERendererType::RAYTRACING; }

private:
	static constexpr VkFormat get_image_format() { return VK_FORMAT_R32G32B32A32_SFLOAT; }
	void create_render_pass();
	VkDescriptorSet create_rt_dset(VkImageView rt_image_view);

	std::vector<RenderingAttachment> color_attachments;
	std::vector<RenderingAttachment> depth_attachments;
	std::vector<VkDescriptorSet> rt_dsets;
	std::vector<VkImage> presentation_images; // owned by the swapchain
};