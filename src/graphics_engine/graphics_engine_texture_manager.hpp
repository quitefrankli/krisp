#pragma once

#include "graphics_engine_base_module.hpp"
#include "utility.hpp"
#include "renderable/material_group.hpp"
#include "environment_lighting.hpp"
#include "graphics_engine_texture.hpp"

#include <quill/LogMacros.h>

#include <map>
#include <optional>
#include <unordered_map>
#include <string>


struct ProcessedEnvironment;
struct ProcessedEnvironmentImage;

class GraphicsEngineTextureManager : public GraphicsEngineBaseModule
{
public:
	GraphicsEngineTextureManager(GraphicsEngine& engine);
	~GraphicsEngineTextureManager();

	// automatically generates texture if requested texture does not exist
	GraphicsEngineTexture& fetch_texture(
		const MaterialHandle& material_owner,
		PbrMaterial::TextureSampler sampler_type);
	// automatically generates sampler if requested sampler type does not exist
	VkSampler fetch_sampler(PbrMaterial::TextureSampler sampler_type);
	GraphicsEngineTexture& fetch_neutral_texture(ETextureSemantic semantic);

	// probably not a great name, this is NOT a 3D texture, but only for cubemaps
	// in the future when we want proper 3D textures this should get renamed
	GraphicsEngineTexture& fetch_cubemap_texture(const CubeMapMatGroup& material_group);
	const EnvironmentLightingTextures& fetch_environment_lighting(
		RenderableID source,
		const CubeMapMatGroup& material_group);
	const EnvironmentLightingTextures& fetch_neutral_environment_lighting();

	void free_texture(MaterialID id);

private:
	VkSampler create_texture_sampler(PbrMaterial::TextureSampler sampler_type);
	GraphicsEngineTexture create_texture(
		const TextureMaterial& material, PbrMaterial::TextureSampler sampler_type);
	glm::uvec3 create_texture_image(
		const TextureMaterial& material, 
		VkImage& texture_image,
		VkDeviceMemory& texture_image_memory);
	void create_cubemap_texture_image(
		const CubeMapMatGroup& material_group,
		VkImage& texture_image,
		VkDeviceMemory& texture_image_memory);
	EnvironmentLightingTextures create_environment_lighting(
		const ProcessedEnvironment& environment);
	GraphicsEngineTexture create_environment_texture(
		const ProcessedEnvironmentImage& image,
		VkImageViewType view_type,
		PbrMaterial::TextureSampler sampler_type);

private:
	// note that this is not deleted even when an object referencing this texture gets destroyed
	std::unordered_map<MaterialID, GraphicsEngineTexture> texture_units;
	std::unordered_map<ETextureSemantic, GraphicsEngineTexture> neutral_textures;
	std::unordered_map<RenderableID, EnvironmentLightingTextures> environment_lighting;
	std::optional<EnvironmentLightingTextures> neutral_environment_lighting;
	std::map<PbrMaterial::TextureSampler, VkSampler> samplers;
};
