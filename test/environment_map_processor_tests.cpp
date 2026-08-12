#include "graphics_engine/environment_map_processor.hpp"
#include "graphics_engine/environment_map_asset.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
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

void expect_same_image(
	const ProcessedEnvironmentImage& actual,
	const ProcessedEnvironmentImage& expected)
{
	EXPECT_EQ(actual.width, expected.width);
	EXPECT_EQ(actual.height, expected.height);
	EXPECT_EQ(actual.layer_count, expected.layer_count);
	EXPECT_EQ(actual.mip_sizes, expected.mip_sizes);
	EXPECT_EQ(actual.rgba8_linear, expected.rgba8_linear);
}

class TemporaryEnvironmentAsset
{
public:
	TemporaryEnvironmentAsset() :
		path(std::filesystem::temp_directory_path() / "krisp_environment_map_asset_test.krisp-ibl")
	{
		std::filesystem::remove(path);
	}
	~TemporaryEnvironmentAsset() { std::filesystem::remove(path); }

	std::filesystem::path path;
};
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

TEST(EnvironmentMapAsset, round_trips_a_processed_environment)
{
	std::array<std::vector<std::byte>, 6> storage;
	const auto faces = constant_faces(storage, 128);
	const auto settings = compact_settings();
	const auto expected = EnvironmentMapProcessor::process(faces, settings);
	TemporaryEnvironmentAsset asset;

	EnvironmentMapAsset::write(asset.path, faces, expected, settings);
	const auto actual = EnvironmentMapAsset::read(asset.path, faces, settings);

	expect_same_image(actual.irradiance, expected.irradiance);
	expect_same_image(actual.prefiltered_specular, expected.prefiltered_specular);
	expect_same_image(actual.brdf_lut, expected.brdf_lut);
}

TEST(EnvironmentMapAsset, rejects_an_asset_for_different_source_pixels)
{
	std::array<std::vector<std::byte>, 6> storage;
	auto faces = constant_faces(storage, 128);
	const auto settings = compact_settings();
	const auto processed = EnvironmentMapProcessor::process(faces, settings);
	TemporaryEnvironmentAsset asset;
	EnvironmentMapAsset::write(asset.path, faces, processed, settings);

	storage.front().front() = std::byte{127};
	EXPECT_THROW(EnvironmentMapAsset::read(asset.path, faces, settings), std::runtime_error);
}
