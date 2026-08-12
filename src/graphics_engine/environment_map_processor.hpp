#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>


struct EnvironmentFace
{
	uint32_t width = 0;
	uint32_t height = 0;
	std::span<const std::byte> rgba8_srgb;
};

// Mips are stored from largest to smallest. Within each mip, cubemap layers
// are contiguous in +X, -X, +Y, -Y, +Z, -Z order.
struct ProcessedEnvironmentImage
{
	uint32_t width = 0;
	uint32_t height = 0;
	uint32_t layer_count = 0;
	std::vector<size_t> mip_sizes;
	std::vector<std::byte> rgba8_linear;
};

struct ProcessedEnvironment
{
	ProcessedEnvironmentImage irradiance;
	ProcessedEnvironmentImage prefiltered_specular;
	ProcessedEnvironmentImage brdf_lut;
};

// Converts one display-ready sRGB cubemap into the three linear resources used
// by split-sum metallic-roughness image-based lighting.
class EnvironmentMapProcessor
{
public:
	struct Settings
	{
		uint32_t irradiance_size = 32;
		uint32_t prefiltered_size = 128;
		uint32_t brdf_lut_size = 128;
		uint32_t irradiance_sample_count = 64;
		uint32_t prefiltered_sample_count = 128;
		uint32_t brdf_sample_count = 128;
	};

	static ProcessedEnvironment process(
		const std::array<EnvironmentFace, 6>& faces);
	static ProcessedEnvironment process(
		const std::array<EnvironmentFace, 6>& faces,
		const Settings& settings);
};
