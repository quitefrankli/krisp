#include "environment_map_asset.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <string>
#include <type_traits>


namespace
{
constexpr std::array<char, 8> MAGIC{'K', 'R', 'S', 'P', 'I', 'B', 'L', '1'};
constexpr uint32_t MAX_MIP_COUNT = 32;
constexpr uint64_t MAX_IMAGE_BYTES = 1024ull * 1024ull * 1024ull;

class Fingerprint
{
public:
	void add_u32(const uint32_t value)
	{
		for (uint32_t shift = 0; shift < 32; shift += 8)
			add_byte(static_cast<uint8_t>(value >> shift));
	}

	void add_bytes(const std::span<const std::byte> bytes)
	{
		for (const std::byte value : bytes)
			add_byte(std::to_integer<uint8_t>(value));
	}

	uint64_t value() const { return hash; }

private:
	void add_byte(const uint8_t value)
	{
		hash ^= value;
		hash *= 1099511628211ull;
	}

	uint64_t hash = 14695981039346656037ull;
};

uint64_t source_fingerprint(const std::array<EnvironmentFace, 6> &faces,
                            const EnvironmentMapProcessor::Settings &settings)
{
	Fingerprint fingerprint;
	for (const uint32_t value :
	     {settings.irradiance_size, settings.prefiltered_size, settings.brdf_lut_size, settings.irradiance_sample_count,
	      settings.prefiltered_sample_count, settings.brdf_sample_count})
	{
		fingerprint.add_u32(value);
	}
	for (const auto &face : faces)
	{
		fingerprint.add_u32(face.width);
		fingerprint.add_u32(face.height);
		fingerprint.add_bytes(face.rgba8_srgb);
	}
	return fingerprint.value();
}

template<typename Integer> void write_integer(std::ostream &stream, const Integer value)
{
	static_assert(std::is_unsigned_v<Integer>);
	for (size_t index = 0; index < sizeof(Integer); ++index)
		stream.put(static_cast<char>(value >> (index * 8)));
}

template<typename Integer> Integer read_integer(std::istream &stream)
{
	static_assert(std::is_unsigned_v<Integer>);
	Integer value = 0;
	for (size_t index = 0; index < sizeof(Integer); ++index)
	{
		const int byte = stream.get();
		if (byte == std::char_traits<char>::eof())
			throw std::runtime_error("environment map asset is truncated");
		value |= static_cast<Integer>(static_cast<uint8_t>(byte)) << (index * 8);
	}
	return value;
}

size_t total_size(const ProcessedEnvironmentImage &image)
{
	if (image.width == 0 || image.height == 0 || image.layer_count == 0 || image.mip_sizes.empty() ||
	    image.mip_sizes.size() > MAX_MIP_COUNT)
	{
		throw std::runtime_error("environment map asset has invalid image metadata");
	}
	size_t total = 0;
	for (const size_t mip_size : image.mip_sizes)
	{
		if (mip_size == 0 || total > std::numeric_limits<size_t>::max() - mip_size)
			throw std::runtime_error("environment map asset image size overflows");
		total += mip_size;
	}
	if (total != image.rgba8_linear.size() || total > MAX_IMAGE_BYTES)
		throw std::runtime_error("environment map asset image data does not match its metadata");
	return total;
}

void require_layout(const ProcessedEnvironment &environment, const EnvironmentMapProcessor::Settings &settings)
{
	const auto complete_mip_count = [](uint32_t size) {
		uint32_t count = 1;
		while (size > 1)
		{
			size >>= 1;
			++count;
		}
		return count;
	};
	if (environment.irradiance.width != settings.irradiance_size ||
	    environment.irradiance.height != settings.irradiance_size || environment.irradiance.layer_count != 6 ||
	    environment.irradiance.mip_sizes.size() != 1 ||
	    environment.prefiltered_specular.width != settings.prefiltered_size ||
	    environment.prefiltered_specular.height != settings.prefiltered_size ||
	    environment.prefiltered_specular.layer_count != 6 ||
	    environment.prefiltered_specular.mip_sizes.size() != complete_mip_count(settings.prefiltered_size) ||
	    environment.brdf_lut.width != settings.brdf_lut_size || environment.brdf_lut.height != settings.brdf_lut_size ||
	    environment.brdf_lut.layer_count != 1 || environment.brdf_lut.mip_sizes.size() != 1)
	{
		throw std::runtime_error("environment map asset layout does not match its settings");
	}
	total_size(environment.irradiance);
	total_size(environment.prefiltered_specular);
	total_size(environment.brdf_lut);
}

void write_image(std::ostream &stream, const ProcessedEnvironmentImage &image)
{
	write_integer(stream, image.width);
	write_integer(stream, image.height);
	write_integer(stream, image.layer_count);
	write_integer(stream, static_cast<uint32_t>(image.mip_sizes.size()));
	for (const size_t mip_size : image.mip_sizes)
		write_integer(stream, static_cast<uint64_t>(mip_size));
	write_integer(stream, static_cast<uint64_t>(image.rgba8_linear.size()));
	stream.write(reinterpret_cast<const char *>(image.rgba8_linear.data()),
	             static_cast<std::streamsize>(image.rgba8_linear.size()));
}

ProcessedEnvironmentImage read_image(std::istream &stream)
{
	ProcessedEnvironmentImage image{
		.width = read_integer<uint32_t>(stream),
		.height = read_integer<uint32_t>(stream),
		.layer_count = read_integer<uint32_t>(stream),
	};
	const uint32_t mip_count = read_integer<uint32_t>(stream);
	if (mip_count == 0 || mip_count > MAX_MIP_COUNT)
		throw std::runtime_error("environment map asset has invalid mip count");
	image.mip_sizes.reserve(mip_count);
	for (uint32_t mip = 0; mip < mip_count; ++mip)
	{
		const uint64_t mip_size = read_integer<uint64_t>(stream);
		if (mip_size > std::numeric_limits<size_t>::max())
			throw std::runtime_error("environment map asset mip is too large");
		image.mip_sizes.push_back(static_cast<size_t>(mip_size));
	}
	const uint64_t data_size = read_integer<uint64_t>(stream);
	if (data_size > MAX_IMAGE_BYTES || data_size > std::numeric_limits<size_t>::max())
		throw std::runtime_error("environment map asset image is too large");
	image.rgba8_linear.resize(static_cast<size_t>(data_size));
	stream.read(reinterpret_cast<char *>(image.rgba8_linear.data()),
	            static_cast<std::streamsize>(image.rgba8_linear.size()));
	if (!stream)
		throw std::runtime_error("environment map asset is truncated");
	total_size(image);
	return image;
}
} // namespace


ProcessedEnvironment EnvironmentMapAsset::read(const std::filesystem::path &path,
                                               const std::array<EnvironmentFace, 6> &source_faces,
                                               const EnvironmentMapProcessor::Settings &settings)
{
	std::ifstream stream(path, std::ios::binary);
	if (!stream)
		throw std::runtime_error("unable to open environment map asset: " + path.string());

	std::array<char, MAGIC.size()> magic;
	stream.read(magic.data(), static_cast<std::streamsize>(magic.size()));
	if (!stream || magic != MAGIC)
		throw std::runtime_error("environment map asset has an unsupported format: " + path.string());
	if (read_integer<uint64_t>(stream) != source_fingerprint(source_faces, settings))
		throw std::runtime_error("environment map asset is stale for its source cubemap: " + path.string());

	ProcessedEnvironment environment{
		.irradiance = read_image(stream),
		.prefiltered_specular = read_image(stream),
		.brdf_lut = read_image(stream),
	};
	if (stream.peek() != std::char_traits<char>::eof())
		throw std::runtime_error("environment map asset has trailing data: " + path.string());
	require_layout(environment, settings);
	return environment;
}

void EnvironmentMapAsset::write(const std::filesystem::path &path, const std::array<EnvironmentFace, 6> &source_faces,
                                const ProcessedEnvironment &environment,
                                const EnvironmentMapProcessor::Settings &settings)
{
	require_layout(environment, settings);
	std::ofstream stream(path, std::ios::binary | std::ios::trunc);
	if (!stream)
		throw std::runtime_error("unable to create environment map asset: " + path.string());
	stream.write(MAGIC.data(), static_cast<std::streamsize>(MAGIC.size()));
	write_integer(stream, source_fingerprint(source_faces, settings));
	write_image(stream, environment.irradiance);
	write_image(stream, environment.prefiltered_specular);
	write_image(stream, environment.brdf_lut);
	if (!stream)
		throw std::runtime_error("unable to write environment map asset: " + path.string());
}
