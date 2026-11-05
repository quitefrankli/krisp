#include "texture_compositor.hpp"

#include "dynamic_rendering.hpp"
#include "graphics_engine.hpp"
#include "graphics_engine_texture_manager.hpp"
#include "pipeline/pipeline.hpp"
#include "resource_manager/graphics_resource_manager.hpp"
#include "shared_data_structures.hpp"

#include <glm/vec4.hpp>

#include <stdexcept>


namespace
{
class TextureCompositorPipeline final : public GraphicsEnginePipeline
{
public:
	TextureCompositorPipeline(GraphicsEngine& engine, const VkExtent2D extent_) :
		GraphicsEnginePipeline(engine), extent(extent_)
	{
		initialise();
	}

protected:
	std::string_view get_shader_name() const override { return "texture_compositor"; }
	std::vector<VkVertexInputBindingDescription> get_binding_descriptions() const override { return {}; }
	std::vector<VkVertexInputAttributeDescription> get_attribute_descriptions() const override { return {}; }
	RenderingFormats get_rendering_formats() override { return { .color = VK_FORMAT_R8G8B8A8_SRGB }; }
	VkExtent2D get_extent() override { return extent; }
	VkSampleCountFlagBits get_msaa_sample_count() override { return VK_SAMPLE_COUNT_1_BIT; }
	VkCullModeFlags get_cull_mode() const override { return VK_CULL_MODE_NONE; }
	std::vector<VkDescriptorSetLayout> get_expected_dset_layouts() override
	{
		return { get_graphics_engine().get_texture_compositor().get_dset_layout() };
	}
	std::vector<VkPushConstantRange> get_push_constant_ranges() const override
	{
		return { VkPushConstantRange{
			.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
			.offset = 0,
			.size = sizeof(SDS::TextureCompositorPushConstant),
		} };
	}
	VkPipelineDepthStencilStateCreateInfo get_depth_stencil_create_info() const override
	{
		VkPipelineDepthStencilStateCreateInfo info{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
		info.depthTestEnable = VK_FALSE;
		info.depthWriteEnable = VK_FALSE;
		info.stencilTestEnable = VK_FALSE;
		return info;
	}
	void mod_color_blend_attachment(VkPipelineColorBlendAttachmentState& blend) const override
	{
		blend.blendEnable = VK_TRUE;
		blend.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
		blend.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		blend.colorBlendOp = VK_BLEND_OP_ADD;
		blend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		blend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		blend.alphaBlendOp = VK_BLEND_OP_ADD;
	}

private:
	VkExtent2D extent;
};

uint64_t extent_key(const VkExtent2D extent)
{
	return (static_cast<uint64_t>(extent.width) << 32) | extent.height;
}
}

TextureCompositor::TextureCompositor(GraphicsEngine& engine) :
	GraphicsEngineBaseModule(engine)
{
	VkDescriptorSetLayoutBinding binding{};
	binding.binding = 0;
	binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	binding.descriptorCount = 1;
	binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	dset_layout = get_rsrc_mgr().request_dset_layout({ binding });
}

TextureCompositor::~TextureCompositor()
{
	pipelines.clear();
	for (auto& [_, resources] : compositions)
		destroy_resources(resources);
}

GraphicsTextureSample TextureCompositor::resolve(
	const MaterialHandle& owner, const PbrMaterial::TextureSampler sampler)
{
	const auto* material = dynamic_cast<const CompositedTextureMaterial*>(&owner->get());
	if (!material)
		throw std::invalid_argument("TextureCompositor: material is not composited");
	const MaterialID id = owner->get_id();
	if (!compositions.contains(id))
	{
		compositions.emplace(id, create_resources(*material));
		pending.push_back(id);
	}
	const auto& resources = compositions.at(id);
	return {
		resources.image_view,
		get_graphics_engine().get_texture_mgr().fetch_sampler(sampler),
	};
}

TextureCompositor::CompositionResources TextureCompositor::create_resources(
	const CompositedTextureMaterial& material)
{
	CompositionResources resources;
	resources.extent = { material.width, material.height };
	resources.layers = material.layers;
	try
	{
		get_graphics_engine().create_image(
			material.width, material.height, VK_FORMAT_R8G8B8A8_SRGB,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, resources.image, resources.allocation,
			VK_SAMPLE_COUNT_1_BIT);
		resources.image_view = get_graphics_engine().create_image_view(
			resources.image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT);
		for (const auto& layer : material.layers)
		{
			const auto& source = get_graphics_engine().get_texture_mgr().fetch_texture(
				layer.source, PbrMaterial::TextureSampler::clamp_to_edge());
			const VkDescriptorSet dset = get_rsrc_mgr().reserve_dset(dset_layout);
			resources.layer_dsets.push_back(dset);
			VkDescriptorImageInfo image_info{};
			image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			image_info.imageView = source.get_texture_image_view();
			image_info.sampler = source.get_texture_sampler();
			VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
			write.dstSet = dset;
			write.dstBinding = 0;
			write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			write.descriptorCount = 1;
			write.pImageInfo = &image_info;
			vkUpdateDescriptorSets(get_logical_device(), 1, &write, 0, nullptr);
		}
	}
	catch (...)
	{
		destroy_resources(resources);
		throw;
	}
	return resources;
}

void TextureCompositor::destroy_resources(CompositionResources& resources)
{
	get_rsrc_mgr().free_dsets(resources.layer_dsets);
	if (resources.image_view != VK_NULL_HANDLE)
		vkDestroyImageView(get_logical_device(), resources.image_view, nullptr);
	if (resources.image != VK_NULL_HANDLE && resources.allocation != nullptr)
		vmaDestroyImage(get_graphics_engine().get_allocator(), resources.image, resources.allocation);
	resources = {};
}

GraphicsEnginePipeline& TextureCompositor::fetch_pipeline(const VkExtent2D extent)
{
	const uint64_t key = extent_key(extent);
	if (!pipelines.contains(key))
		pipelines.emplace(key, std::make_unique<TextureCompositorPipeline>(
			get_graphics_engine(), extent));
	return *pipelines.at(key);
}

void TextureCompositor::record_pending(const VkCommandBuffer command_buffer)
{
	// This records each immutable composition exactly once, when it first becomes
	// live. The per-layer push constants below are generation inputs, not
	// per-frame material state; subsequent frames sample the finished GPU image.
	for (const MaterialID id : pending)
	{
		const auto found = compositions.find(id);
		if (found == compositions.end())
			continue;
		auto& resources = found->second;
		auto& pipeline = fetch_pipeline(resources.extent);
		command_image_barrier(command_buffer, resources.image, VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
		VkRenderingAttachmentInfo color_attachment{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
		color_attachment.imageView = resources.image_view;
		color_attachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		color_attachment.clearValue.color.float32[0] = 0.0f;
		color_attachment.clearValue.color.float32[1] = 0.0f;
		color_attachment.clearValue.color.float32[2] = 0.0f;
		color_attachment.clearValue.color.float32[3] = 0.0f;
		VkRenderingInfo rendering_info{VK_STRUCTURE_TYPE_RENDERING_INFO};
		rendering_info.renderArea = { { 0, 0 }, resources.extent };
		rendering_info.layerCount = 1;
		rendering_info.colorAttachmentCount = 1;
		rendering_info.pColorAttachments = &color_attachment;
		vkCmdBeginRendering(command_buffer, &rendering_info);
		vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.graphics_pipeline);
		for (size_t index = 0; index < resources.layers.size(); ++index)
		{
			const auto& layer = resources.layers[index];
			// Pushed once for this layer's one-time full-screen generation draw.
			const SDS::TextureCompositorPushConstant push{
				.placement = glm::vec4(layer.centre, layer.scale),
				.tint_opacity = glm::vec4(layer.tint, layer.opacity),
				.rotation_radians = layer.rotation_radians,
			};
			vkCmdPushConstants(command_buffer, pipeline.pipeline_layout,
				VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push), &push);
			vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
				pipeline.pipeline_layout, 0, 1, &resources.layer_dsets[index], 0, nullptr);
			vkCmdDraw(command_buffer, 3, 1, 0, 0);
		}
		vkCmdEndRendering(command_buffer);
		command_image_barrier(command_buffer, resources.image, VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT);
	}
	pending.clear();
}

void TextureCompositor::free_texture(const MaterialID id)
{
	const auto found = compositions.find(id);
	if (found == compositions.end())
		return;
	destroy_resources(found->second);
	compositions.erase(found);
}
