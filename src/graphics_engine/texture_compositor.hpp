#pragma once

#include "graphics_engine_base_module.hpp"
#include "renderable/composited_texture_material.hpp"

#include <vulkan/vulkan.hpp>

#include <memory>
#include <unordered_map>
#include <vector>


class GraphicsEnginePipeline;

// Sampling handles returned to the normal material-descriptor path. The
// compositor retains ownership of both objects for the composition's lifetime.
struct GraphicsTextureSample
{
	VkImageView image_view = VK_NULL_HANDLE;
	VkSampler sampler = VK_NULL_HANDLE;
};

// Generates immutable CompositedTextureMaterial recipes into GPU-only textures.
//
// This is intentionally not a Renderer: renderers produce swap-chain-indexed
// output every frame, whereas a composition has its own material-defined extent
// and is rendered only once. resolve() allocates and caches an empty output;
// record_pending() fills newly allocated outputs before the shadow and scene
// passes. Normal material draws then sample the stable cached image directly.
// Resources are keyed by MaterialID so shared materials share one generated
// texture and can use the engine's submission-safe material retirement path.
class TextureCompositor : public GraphicsEngineBaseModule
{
public:
	explicit TextureCompositor(GraphicsEngine& engine);
	~TextureCompositor();

	// Returns stable sampling handles for a composition, allocating its GPU
	// output and queueing one-time generation on first use.
	GraphicsTextureSample resolve(const MaterialHandle& material);
	// Records all queued layer stacks into the current graphics command buffer.
	// Command ordering makes the completed images visible to later scene passes.
	void record_pending(VkCommandBuffer command_buffer);
	// Releases a retired material's compositor-owned resources. The caller must
	// invoke this only after the last referencing submission has completed.
	void free_texture(MaterialID id);

	VkRenderPass get_render_pass() const { return render_pass; }
	VkDescriptorSetLayout get_dset_layout() const { return dset_layout; }

private:
	// Complete persistent state for one generated material. Each source layer has
	// a descriptor set; all layer draws target the same output framebuffer using
	// premultiplied source-over blending. No CPU pixel copy or mip chain exists.
	struct CompositionResources
	{
		VkImage image = VK_NULL_HANDLE;
		VkDeviceMemory memory = VK_NULL_HANDLE;
		VkImageView image_view = VK_NULL_HANDLE;
		VkFramebuffer framebuffer = VK_NULL_HANDLE;
		VkSampler sampler = VK_NULL_HANDLE;
		VkExtent2D extent{};
		std::vector<VkDescriptorSet> layer_dsets;
		std::vector<TextureCompositionLayer> layers;
	};

	void create_render_pass();
	CompositionResources create_resources(const CompositedTextureMaterial& material);
	void destroy_resources(CompositionResources& resources);
	GraphicsEnginePipeline& fetch_pipeline(VkExtent2D extent);

	// All outputs share format and render-pass compatibility, while pipelines are
	// cached by extent because the current pipeline abstraction uses static
	// viewport/scissor state.
	VkRenderPass render_pass = VK_NULL_HANDLE;
	VkDescriptorSetLayout dset_layout = VK_NULL_HANDLE;
	std::unordered_map<MaterialID, CompositionResources> compositions;
	std::unordered_map<uint64_t, std::unique_ptr<GraphicsEnginePipeline>> pipelines;
	// Newly resolved immutable compositions wait here for one-time GPU generation.
	// Once recorded, later frames only sample their cached output images.
	std::vector<MaterialID> pending;
};
