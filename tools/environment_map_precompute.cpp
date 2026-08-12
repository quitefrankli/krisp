#include "environment_map_asset.hpp"
#include "environment_map_processor.hpp"

#include <stb_image.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>


namespace
{
struct SourceFace
{
	uint32_t width = 0;
	uint32_t height = 0;
	std::vector<std::byte> pixels;
};

SourceFace load_face(const std::filesystem::path &path)
{
	int width = 0;
	int height = 0;
	int source_channels = 0;
	stbi_uc *decoded = stbi_load(path.string().c_str(), &width, &height, &source_channels, STBI_rgb_alpha);
	if (!decoded)
		throw std::runtime_error("unable to decode cubemap face: " + path.string());
	if (width <= 0 || height <= 0)
	{
		stbi_image_free(decoded);
		throw std::runtime_error("cubemap face has invalid dimensions: " + path.string());
	}

	SourceFace face{
		.width = static_cast<uint32_t>(width),
		.height = static_cast<uint32_t>(height),
	};
	const size_t byte_count = static_cast<size_t>(width) * height * 4;
	face.pixels.resize(byte_count);
	std::memcpy(face.pixels.data(), decoded, byte_count);
	stbi_image_free(decoded);
	return face;
}
} // namespace


int main(const int argc, const char *const *argv)
{
	try
	{
		if (argc != 8)
		{
			std::cerr << "usage: environment_map_precompute OUTPUT +X -X +Y -Y +Z -Z\n";
			return 2;
		}

		std::array<SourceFace, 6> storage;
		std::array<EnvironmentFace, 6> faces;
		for (size_t index = 0; index < faces.size(); ++index)
		{
			storage[index] = load_face(argv[index + 2]);
			faces[index] = {
				.width = storage[index].width,
				.height = storage[index].height,
				.rgba8_srgb = storage[index].pixels,
			};
		}

		const auto processed = EnvironmentMapProcessor::process(faces);
		EnvironmentMapAsset::write(argv[1], faces, processed);
		return 0;
	} catch (const std::exception &error)
	{
		std::cerr << "environment_map_precompute: " << error.what() << '\n';
		return 1;
	}
}
