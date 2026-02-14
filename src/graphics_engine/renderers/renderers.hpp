#pragma once

#include "renderer.hpp"
#include "constants.hpp"

#include <optional>


class ParticleRenderer;

class RasterizationRenderer : public Renderer
{
public:
	RasterizationRenderer(GraphicsEngine& engine);
	~RasterizationRenderer();

	virtual void allocate_per_frame_resources(VkImage presentation_image, VkImageView presentation_image_view) override;
	virtual void submit_draw_commands(VkCommandBuffer command_buffer, VkImageView presentation_image_view, uint32_t frame_index) override;
	virtual constexpr ERendererType get_renderer_type() const override { return ERendererType::RASTERIZATION; }
	virtual VkImageView get_output_image_view(uint32_t) override { return nullptr; };
	void set_shadow_map_inputs(const std::vector<VkImageView>& shadow_map_inputs);

	// Allow particle renderer to draw within our render pass
	friend class ParticleRenderer;

private:
	using Renderer::get_graphics_engine;
	using Renderer::get_rsrc_mgr;
	using Renderer::get_logical_device;

	static constexpr VkFormat get_image_format() { return VK_FORMAT_B8G8R8A8_SRGB; }
	void create_render_pass();

	std::vector<RenderingAttachment> color_attachments;
	std::vector<RenderingAttachment> depth_attachments;
	// presentation attachment which is also responsible for msaa resolve, is owned by the swapchain

	std::vector<VkDescriptorSet> shadow_map_dsets;

	VkSampler shadow_map_sampler;
};

class GuiRenderer : public Renderer
{
public:
	GuiRenderer(GraphicsEngine& engine);
	~GuiRenderer();

	virtual void allocate_per_frame_resources(VkImage presentation_image, VkImageView presentation_image_view) override;
	virtual void submit_draw_commands(VkCommandBuffer command_buffer, VkImageView presentation_image_view, uint32_t frame_index) override;
	virtual constexpr ERendererType get_renderer_type() const override { return ERendererType::GUI; }
	virtual VkImageView get_output_image_view(uint32_t) override { return nullptr; };

private:
	static constexpr VkFormat get_image_format() { return VK_FORMAT_B8G8R8A8_SRGB; }
	void create_render_pass();

	using Renderer::get_graphics_engine;
	using Renderer::get_rsrc_mgr;
	using Renderer::get_logical_device;
};

class RaytracingRenderer : public Renderer
{
public:
	RaytracingRenderer(GraphicsEngine& engine);
	~RaytracingRenderer();

	virtual void allocate_per_frame_resources(VkImage presentation_image, VkImageView presentation_image_view) override;
	virtual void submit_draw_commands(VkCommandBuffer command_buffer, VkImageView presentation_image_view, uint32_t frame_index) override;
	virtual constexpr ERendererType get_renderer_type() const override { return ERendererType::RAYTRACING; }
	virtual VkImageView get_output_image_view(uint32_t) override { return nullptr; };

	void update_rt_dsets();

private:
	static constexpr VkFormat get_image_format() { return VK_FORMAT_R32G32B32A32_SFLOAT; }
	void create_render_pass();
	VkDescriptorSet create_rt_dset(VkImageView rt_image_view);

	std::vector<RenderingAttachment> color_attachments;
	std::vector<RenderingAttachment> depth_attachments;
	std::vector<VkDescriptorSet> rt_dsets;
	std::vector<VkImage> presentation_images; // owned by the swapchain

	using Renderer::get_graphics_engine;
	using Renderer::get_rsrc_mgr;
	using Renderer::get_logical_device;
};

// for things such as minimaps, object previews e.t.c.
class OffscreenGuiViewportRenderer : public Renderer
{
public:
	OffscreenGuiViewportRenderer(GraphicsEngine& engine);
	~OffscreenGuiViewportRenderer();

	virtual void allocate_per_frame_resources(VkImage, VkImageView) override;
	virtual void submit_draw_commands(VkCommandBuffer command_buffer, VkImageView, uint32_t frame_index) override;
	virtual constexpr ERendererType get_renderer_type() const override { return ERendererType::OFFSCREEN_GUI_VIEWPORT; }
	virtual VkImageView get_output_image_view(uint32_t frame_idx) override { return color_attachments[frame_idx].image_view; };
	virtual VkExtent2D get_extent() override;

private:
	static constexpr VkFormat get_image_format() { return VK_FORMAT_B8G8R8A8_SRGB; }
	virtual VkSampleCountFlagBits get_msaa_sample_count() const override { return VK_SAMPLE_COUNT_1_BIT; }
	void create_render_pass();

	std::vector<RenderingAttachment> color_attachments;
	std::vector<RenderingAttachment> depth_attachments;

	using Renderer::get_graphics_engine;
	using Renderer::get_rsrc_mgr;
	using Renderer::get_logical_device;
};

class ShadowMapRenderer : public Renderer
{
public:
	ShadowMapRenderer(GraphicsEngine& engine);
	~ShadowMapRenderer();

	virtual void allocate_per_frame_resources(VkImage, VkImageView) override;
	virtual void submit_draw_commands(VkCommandBuffer command_buffer, VkImageView, uint32_t frame_index) override;
	virtual constexpr ERendererType get_renderer_type() const override { return ERendererType::OFFSCREEN_GUI_VIEWPORT; }
	virtual VkImageView get_output_image_view(uint32_t frame_idx) override { return shadow_map_cube_views[frame_idx]; };
	virtual VkExtent2D get_extent() override { return { 1024, 1024 }; } // resolution of each face of the shadow map

	VkDescriptorSet get_shadow_map_dset(uint32_t frame_idx) { return shadow_map_dsets[frame_idx]; }

private:
	static constexpr VkFormat get_image_format() { return VK_FORMAT_D32_SFLOAT; }
	virtual VkSampleCountFlagBits get_msaa_sample_count() const override { return VK_SAMPLE_COUNT_1_BIT; }
	void create_render_pass();
	void create_sampler();
	void create_shadow_map_dset(VkImageView shadow_map_view);

	std::vector<RenderingAttachment> shadow_map_attachments;
	std::vector<VkImageView> shadow_map_cube_views;
	std::vector<VkDescriptorSet> shadow_map_dsets;
	VkSampler shadow_map_sampler;

	using Renderer::get_graphics_engine;
	using Renderer::get_rsrc_mgr;
	using Renderer::get_logical_device;
};

class QuadRenderer : public Renderer
{
public:
	QuadRenderer(GraphicsEngine& engine);
	~QuadRenderer();

	virtual void allocate_per_frame_resources(VkImage, VkImageView) override;
	virtual void submit_draw_commands(VkCommandBuffer command_buffer, VkImageView, uint32_t frame_index) override;
	virtual constexpr ERendererType get_renderer_type() const override { return ERendererType::QUAD; }
	virtual VkImageView get_output_image_view(uint32_t frame_idx) override { return color_attachments[frame_idx].image_view; };
	virtual VkExtent2D get_extent() override { return { 512, 512 }; }

	void set_texture(VkImageView texture_view, VkSampler texture_sampler);
	void set_texture_sampling_flags(int flags) { sampling_flags = flags; }

private:
	static constexpr VkFormat get_image_format() { return VK_FORMAT_B8G8R8A8_SRGB; }
	virtual VkSampleCountFlagBits get_msaa_sample_count() const override { return VK_SAMPLE_COUNT_1_BIT; }
	void create_render_pass();

	VkDescriptorSet texture;
	struct TextureToRender
	{
		VkImageView texture_view;
		VkSampler texture_sampler;
		// dset is only updated when this hits 0
		int render_frames_remaining;
	};

	std::optional<TextureToRender> texture_to_render;
	bool should_render = false;

	int sampling_flags = 0;

	std::vector<RenderingAttachment> color_attachments;

	using Renderer::get_graphics_engine;
	using Renderer::get_rsrc_mgr;
	using Renderer::get_logical_device;
};
