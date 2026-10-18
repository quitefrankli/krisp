#pragma once

#include "renderer.hpp"
#include "constants.hpp"
#include "graphics_engine/point_lighting.hpp"

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
	virtual VkImageView get_output_image_view(uint32_t frame_idx) override { return resolve_attachments.at(frame_idx).image_view; };

	// Allow particle renderer to draw within our render pass
	friend class ParticleRenderer;

private:
	using Renderer::get_graphics_engine;
	using Renderer::get_rsrc_mgr;
	using Renderer::get_logical_device;

	static constexpr VkFormat get_image_format() { return VK_FORMAT_R16G16B16A16_SFLOAT; }
	void create_render_pass();

	std::vector<RenderingAttachment> color_attachments;
	std::vector<RenderingAttachment> resolve_attachments;
	std::vector<RenderingAttachment> depth_attachments;
	// The multisampled scene is resolved to resolve_attachments for presentation sampling.

};

class PresentationRenderer : public Renderer
{
public:
	PresentationRenderer(GraphicsEngine& engine);
	~PresentationRenderer();

	void allocate_per_frame_resources(VkImage, VkImageView presentation_image_view) override;
	void submit_draw_commands(VkCommandBuffer command_buffer, VkImageView, uint32_t frame_index) override;
	constexpr ERendererType get_renderer_type() const override { return ERendererType::PRESENTATION; }
	VkImageView get_output_image_view(uint32_t) override { return nullptr; }
	VkSampleCountFlagBits get_msaa_sample_count() const override { return VK_SAMPLE_COUNT_1_BIT; }

private:
	// Converts the resolved linear HDR scene into the SDR sRGB swap-chain image.
	// GUI composition happens in the following render pass and is not tone-mapped.
	void create_render_pass();
	// One immutable scene input descriptor per in-flight swap-chain image avoids
	// descriptor updates while a previous frame may still be using it.
	std::vector<VkDescriptorSet> scene_inputs;
	VkSampler scene_sampler = VK_NULL_HANDLE;
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

class ShadowMapRenderer : public Renderer
{
public:
	ShadowMapRenderer(GraphicsEngine& engine);
	~ShadowMapRenderer();

	virtual void allocate_per_frame_resources(VkImage, VkImageView) override;
	virtual void submit_draw_commands(VkCommandBuffer command_buffer, VkImageView, uint32_t frame_index) override;
	virtual constexpr ERendererType get_renderer_type() const override { return ERendererType::SHADOW_MAP; }
	virtual VkImageView get_output_image_view(uint32_t frame_idx) override { return frames.at(frame_idx).preview_view; }
	virtual VkExtent2D get_extent() override { return { POINT_SHADOW_RESOLUTION, POINT_SHADOW_RESOLUTION }; }
	// Called only after this frame's fence completes, before recording commands.
	void prepare_frame(uint32_t frame_idx, PointLightingData lighting);
	const PointLightingData& get_lighting_data(uint32_t frame_idx) const { return frames.at(frame_idx).lighting; }

	VkDescriptorSet get_shadow_map_dset(uint32_t frame_idx) const { return frames.at(frame_idx).descriptor; }

private:
	static constexpr VkFormat get_image_format() { return VK_FORMAT_D32_SFLOAT; }
	virtual VkSampleCountFlagBits get_msaa_sample_count() const override { return VK_SAMPLE_COUNT_1_BIT; }
	void create_render_pass();
	void create_sampler();
	struct FrameResources
	{
		RenderingAttachment attachment;
		VkImageView cube_array_view = VK_NULL_HANDLE;
		VkImageView preview_view = VK_NULL_HANDLE;
		VkDescriptorSet descriptor = VK_NULL_HANDLE;
		uint32_t capacity = 0;
		PointLightingData lighting;
	};
	void grow_frame_resources(uint32_t frame_idx, uint32_t capacity);
	void destroy_frame_images(FrameResources& frame);
	std::vector<FrameResources> frames;
	VkSampler shadow_map_sampler = VK_NULL_HANDLE;

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
	void set_renderer_source(ERendererType source, VkSampler sampler);
	void set_texture_sampling_flags(int flags) { sampling_flags = flags; }

private:
	static constexpr VkFormat get_image_format() { return VK_FORMAT_B8G8R8A8_SRGB; }
	virtual VkSampleCountFlagBits get_msaa_sample_count() const override { return VK_SAMPLE_COUNT_1_BIT; }
	void create_render_pass();

	std::vector<VkDescriptorSet> textures;
	VkDescriptorSetLayout texture_layout = VK_NULL_HANDLE;
	VkImageView texture_view = VK_NULL_HANDLE;
	VkSampler texture_sampler = VK_NULL_HANDLE;
	std::optional<ERendererType> renderer_source;
	bool should_render = false;

	int sampling_flags = 0;

	std::vector<RenderingAttachment> color_attachments;

	using Renderer::get_graphics_engine;
	using Renderer::get_rsrc_mgr;
	using Renderer::get_logical_device;
};
