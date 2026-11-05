#pragma once

#include "graphics_engine_texture.hpp"

#include <utility>


struct EnvironmentLightingTextures
{
	EnvironmentLightingTextures(
		GraphicsEngineTexture irradiance_,
		GraphicsEngineTexture prefiltered_specular_,
		GraphicsEngineTexture brdf_lut_) :
		irradiance(std::move(irradiance_)),
		prefiltered_specular(std::move(prefiltered_specular_)),
		brdf_lut(std::move(brdf_lut_))
	{
	}

	EnvironmentLightingTextures(EnvironmentLightingTextures&&) noexcept = default;
	EnvironmentLightingTextures(const EnvironmentLightingTextures&) = delete;
	EnvironmentLightingTextures& operator=(const EnvironmentLightingTextures&) = delete;

	void destroy(const VkDevice device, VmaAllocator allocator)
	{
		irradiance.destroy(device, allocator);
		prefiltered_specular.destroy(device, allocator);
		brdf_lut.destroy(device, allocator);
	}

	GraphicsEngineTexture irradiance;
	GraphicsEngineTexture prefiltered_specular;
	GraphicsEngineTexture brdf_lut;
};
