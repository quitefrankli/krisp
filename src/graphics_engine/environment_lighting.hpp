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

	void destroy(const VkDevice device)
	{
		irradiance.destroy(device);
		prefiltered_specular.destroy(device);
		brdf_lut.destroy(device);
	}

	GraphicsEngineTexture irradiance;
	GraphicsEngineTexture prefiltered_specular;
	GraphicsEngineTexture brdf_lut;
};
