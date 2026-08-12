#include "graphics_engine/environment_map_processor.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>


namespace
{
std::array<EnvironmentFace, 6> constant_faces(
	std::array<std::vector<std::byte>, 6>& storage,
	const uint8_t value)
{
	std::array<EnvironmentFace, 6> faces;
	for (size_t index = 0; index < faces.size(); ++index)
	{
		storage[index].resize(2 * 2 * 4);
		for (size_t pixel = 0; pixel < storage[index].size(); pixel += 4)
		{
			storage[index][pixel] = static_cast<std::byte>(value);
			storage[index][pixel + 1] = static_cast<std::byte>(value);
			storage[index][pixel + 2] = static_cast<std::byte>(value);
			storage[index][pixel + 3] = std::byte{255};
		}
		faces[index] = {
			.width = 2,
			.height = 2,
			.rgba8_srgb = storage[index],
		};
	}
	return faces;
}

EnvironmentMapProcessor::Settings compact_settings()
{
	return {
		.irradiance_size = 2,
		.prefiltered_size = 4,
		.brdf_lut_size = 4,
		.irradiance_sample_count = 16,
		.prefiltered_sample_count = 16,
		.brdf_sample_count = 32,
	};
}

void expect_constant_linear_image(
	const ProcessedEnvironmentImage& image,
	const uint8_t expected)
{
	for (size_t offset = 0; offset < image.rgba8_linear.size(); offset += 4)
	{
		EXPECT_NEAR(std::to_integer<uint8_t>(image.rgba8_linear[offset]), expected, 1);
		EXPECT_NEAR(std::to_integer<uint8_t>(image.rgba8_linear[offset + 1]), expected, 1);
		EXPECT_NEAR(std::to_integer<uint8_t>(image.rgba8_linear[offset + 2]), expected, 1);
		EXPECT_EQ(std::to_integer<uint8_t>(image.rgba8_linear[offset + 3]), 255);
	}
}
}


TEST(EnvironmentMapProcessor, produces_complete_linear_ibl_resources)
{
	std::array<std::vector<std::byte>, 6> storage;
	const auto processed = EnvironmentMapProcessor::process(
		constant_faces(storage, 128), compact_settings());

	EXPECT_EQ(processed.irradiance.width, 2);
	EXPECT_EQ(processed.irradiance.height, 2);
	EXPECT_EQ(processed.irradiance.layer_count, 6);
	EXPECT_EQ(processed.irradiance.mip_sizes, std::vector<size_t>({96}));
	EXPECT_EQ(processed.prefiltered_specular.width, 4);
	EXPECT_EQ(processed.prefiltered_specular.layer_count, 6);
	EXPECT_EQ(processed.prefiltered_specular.mip_sizes,
		std::vector<size_t>({384, 96, 24}));
	EXPECT_EQ(processed.brdf_lut.width, 4);
	EXPECT_EQ(processed.brdf_lut.height, 4);
	EXPECT_EQ(processed.brdf_lut.layer_count, 1);
	EXPECT_EQ(processed.brdf_lut.mip_sizes, std::vector<size_t>({64}));

	// sRGB 128 converts to approximately 0.216 linear, or byte value 55.
	expect_constant_linear_image(processed.irradiance, 55);
	expect_constant_linear_image(processed.prefiltered_specular, 55);
	for (size_t offset = 0; offset < processed.brdf_lut.rgba8_linear.size(); offset += 4)
	{
		const uint32_t a = std::to_integer<uint8_t>(processed.brdf_lut.rgba8_linear[offset]);
		const uint32_t b = std::to_integer<uint8_t>(processed.brdf_lut.rgba8_linear[offset + 1]);
		EXPECT_LE(a + b, 256u);
		EXPECT_EQ(processed.brdf_lut.rgba8_linear[offset + 2], std::byte{0});
		EXPECT_EQ(processed.brdf_lut.rgba8_linear[offset + 3], std::byte{255});
	}
	const size_t smooth_normal_offset = 3 * 4;
	EXPECT_GT(std::to_integer<uint8_t>(
		processed.brdf_lut.rgba8_linear[smooth_normal_offset]), 128);
	const size_t smooth_grazing_offset = 0;
	EXPECT_GT(std::to_integer<uint8_t>(
		processed.brdf_lut.rgba8_linear[smooth_grazing_offset + 1]), 16);
}

TEST(EnvironmentMapProcessor, rejects_mismatched_cubemap_faces)
{
	std::array<std::vector<std::byte>, 6> storage;
	auto faces = constant_faces(storage, 255);
	faces.back().width = 1;
	EXPECT_THROW(
		EnvironmentMapProcessor::process(faces, compact_settings()),
		std::invalid_argument);
}
