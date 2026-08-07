#pragma once

#include "graphics_engine_base_module.hpp"
#include "utility.hpp"
#include "renderable/material_group.hpp"
#include "graphics_engine_texture.hpp"

#include <quill/LogMacros.h>

#include <unordered_map>
#include <string>


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

private:
	// note that this is not deleted even when an object referencing this texture gets destroyed
	std::unordered_map<MaterialID, GraphicsEngineTexture> texture_units;
	std::unordered_map<ETextureSemantic, GraphicsEngineTexture> neutral_textures;
	std::unordered_map<PbrMaterial::TextureSampler, VkSampler> samplers;
};
