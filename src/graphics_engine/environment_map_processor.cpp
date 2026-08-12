#include "environment_map_processor.hpp"

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>


namespace
{
constexpr float PI = 3.14159265358979323846f;
constexpr uint32_t CHANNEL_COUNT = 4;
constexpr uint32_t CUBEMAP_LAYER_COUNT = 6;

struct LinearFace
{
	uint32_t width;
	uint32_t height;
	std::vector<glm::vec3> pixels;
};

struct TangentFrame
{
	glm::vec3 normal;
	glm::vec3 tangent;
	glm::vec3 bitangent;
};

const std::array<float, 256> SRGB_TO_LINEAR = []
{
	std::array<float, 256> values;
	for (size_t index = 0; index < values.size(); ++index)
	{
		const float value = static_cast<float>(index) / 255.0f;
		values[index] = value <= 0.04045f
			? value / 12.92f
			: std::pow((value + 0.055f) / 1.055f, 2.4f);
	}
	return values;
}();

std::byte quantize(const float value)
{
	return static_cast<std::byte>(static_cast<uint8_t>(
		std::lround(std::clamp(value, 0.0f, 1.0f) * 255.0f)));
}

std::array<LinearFace, CUBEMAP_LAYER_COUNT> linearize_faces(
	const std::array<EnvironmentFace, CUBEMAP_LAYER_COUNT>& faces)
{
	std::array<LinearFace, CUBEMAP_LAYER_COUNT> linear_faces;
	for (size_t face_index = 0; face_index < faces.size(); ++face_index)
	{
		const auto& face = faces[face_index];
		auto& linear = linear_faces[face_index];
		linear.width = face.width;
		linear.height = face.height;
		linear.pixels.resize(static_cast<size_t>(face.width) * face.height);
		for (size_t pixel = 0; pixel < linear.pixels.size(); ++pixel)
		{
			const size_t offset = pixel * CHANNEL_COUNT;
			linear.pixels[pixel] = {
				SRGB_TO_LINEAR[std::to_integer<uint8_t>(face.rgba8_srgb[offset])],
				SRGB_TO_LINEAR[std::to_integer<uint8_t>(face.rgba8_srgb[offset + 1])],
				SRGB_TO_LINEAR[std::to_integer<uint8_t>(face.rgba8_srgb[offset + 2])],
			};
		}
	}
	return linear_faces;
}

glm::vec3 sample_cubemap(
	const std::array<LinearFace, CUBEMAP_LAYER_COUNT>& faces,
	const glm::vec3 direction)
{
	const glm::vec3 absolute = glm::abs(direction);
	uint32_t face_index;
	float major_axis;
	float face_s;
	float face_t;
	if (absolute.x >= absolute.y && absolute.x >= absolute.z)
	{
		major_axis = absolute.x;
		if (direction.x >= 0.0f)
		{
			face_index = 0;
			face_s = -direction.z;
			face_t = -direction.y;
		}
		else
		{
			face_index = 1;
			face_s = direction.z;
			face_t = -direction.y;
		}
	}
	else if (absolute.y >= absolute.z)
	{
		major_axis = absolute.y;
		if (direction.y >= 0.0f)
		{
			face_index = 2;
			face_s = direction.x;
			face_t = direction.z;
		}
		else
		{
			face_index = 3;
			face_s = direction.x;
			face_t = -direction.z;
		}
	}
	else
	{
		major_axis = absolute.z;
		if (direction.z >= 0.0f)
		{
			face_index = 4;
			face_s = direction.x;
			face_t = -direction.y;
		}
		else
		{
			face_index = 5;
			face_s = -direction.x;
			face_t = -direction.y;
		}
	}

	const auto& face = faces[face_index];
	const float u = std::clamp(0.5f * (face_s / major_axis + 1.0f), 0.0f, 1.0f);
	const float v = std::clamp(0.5f * (face_t / major_axis + 1.0f), 0.0f, 1.0f);
	const float image_x = u * static_cast<float>(face.width - 1);
	const float image_y = v * static_cast<float>(face.height - 1);
	const uint32_t x0 = static_cast<uint32_t>(std::floor(image_x));
	const uint32_t y0 = static_cast<uint32_t>(std::floor(image_y));
	const uint32_t x1 = std::min(x0 + 1, face.width - 1);
	const uint32_t y1 = std::min(y0 + 1, face.height - 1);
	const float blend_x = image_x - static_cast<float>(x0);
	const float blend_y = image_y - static_cast<float>(y0);
	const auto pixel = [&face](const uint32_t x, const uint32_t y)
	{
		return face.pixels[static_cast<size_t>(y) * face.width + x];
	};
	const glm::vec3 top = glm::mix(
		pixel(x0, y0), pixel(x1, y0), blend_x);
	const glm::vec3 bottom = glm::mix(
		pixel(x0, y1), pixel(x1, y1), blend_x);
	return glm::mix(top, bottom, blend_y);
}

glm::vec3 cubemap_texel_direction(
	const uint32_t face,
	const uint32_t x,
	const uint32_t y,
	const uint32_t size)
{
	const float s = 2.0f * (static_cast<float>(x) + 0.5f)
		/ static_cast<float>(size) - 1.0f;
	const float t = 2.0f * (static_cast<float>(y) + 0.5f)
		/ static_cast<float>(size) - 1.0f;
	switch (face)
	{
	case 0: return glm::normalize(glm::vec3(1.0f, -t, -s));
	case 1: return glm::normalize(glm::vec3(-1.0f, -t, s));
	case 2: return glm::normalize(glm::vec3(s, 1.0f, t));
	case 3: return glm::normalize(glm::vec3(s, -1.0f, -t));
	case 4: return glm::normalize(glm::vec3(s, -t, 1.0f));
	case 5: return glm::normalize(glm::vec3(-s, -t, -1.0f));
	default: throw std::logic_error("invalid cubemap face");
	}
}

glm::vec2 hammersley(const uint32_t index, const uint32_t count)
{
	uint32_t reversed = index;
	reversed = (reversed << 16) | (reversed >> 16);
	reversed = ((reversed & 0x55555555u) << 1) | ((reversed & 0xAAAAAAAAu) >> 1);
	reversed = ((reversed & 0x33333333u) << 2) | ((reversed & 0xCCCCCCCCu) >> 2);
	reversed = ((reversed & 0x0F0F0F0Fu) << 4) | ((reversed & 0xF0F0F0F0u) >> 4);
	reversed = ((reversed & 0x00FF00FFu) << 8) | ((reversed & 0xFF00FF00u) >> 8);
	return {
		static_cast<float>(index) / static_cast<float>(count),
		static_cast<float>(reversed) * 2.3283064365386963e-10f,
	};
}

TangentFrame make_tangent_frame(const glm::vec3 normal)
{
	const glm::vec3 reference = std::abs(normal.z) < 0.999f
		? glm::vec3(0.0f, 0.0f, 1.0f)
		: glm::vec3(1.0f, 0.0f, 0.0f);
	const glm::vec3 tangent = glm::normalize(glm::cross(reference, normal));
	return {
		.normal = normal,
		.tangent = tangent,
		.bitangent = glm::cross(normal, tangent),
	};
}

glm::vec3 cosine_hemisphere_sample(
	const glm::vec2 sample,
	const TangentFrame& frame)
{
	const float azimuth = 2.0f * PI * sample.x;
	const float radial = std::sqrt(sample.y);
	const float z = std::sqrt(std::max(0.0f, 1.0f - sample.y));
	return frame.tangent * (std::cos(azimuth) * radial)
		+ frame.bitangent * (std::sin(azimuth) * radial)
		+ frame.normal * z;
}

glm::vec3 importance_sample_ggx(
	const glm::vec2 sample,
	const TangentFrame& frame,
	const float perceptual_roughness)
{
	const float alpha = perceptual_roughness * perceptual_roughness;
	const float alpha_squared = alpha * alpha;
	const float azimuth = 2.0f * PI * sample.x;
	const float cos_theta = std::sqrt((1.0f - sample.y)
		/ (1.0f + (alpha_squared - 1.0f) * sample.y));
	const float sin_theta = std::sqrt(std::max(0.0f, 1.0f - cos_theta * cos_theta));
	return glm::normalize(
		frame.tangent * (std::cos(azimuth) * sin_theta)
		+ frame.bitangent * (std::sin(azimuth) * sin_theta)
		+ frame.normal * cos_theta);
}

size_t mip_byte_size(
	const uint32_t width,
	const uint32_t height,
	const uint32_t layer_count)
{
	size_t texel_count = width;
	if (height != 0 && texel_count > std::numeric_limits<size_t>::max() / height)
		throw std::overflow_error("environment image is too large");
	texel_count *= height;
	if (layer_count != 0
		&& texel_count > std::numeric_limits<size_t>::max() / layer_count)
		throw std::overflow_error("environment image is too large");
	texel_count *= layer_count;
	if (texel_count > std::numeric_limits<size_t>::max() / CHANNEL_COUNT)
		throw std::overflow_error("environment image is too large");
	return texel_count * CHANNEL_COUNT;
}

ProcessedEnvironmentImage make_image(
	const uint32_t width,
	const uint32_t height,
	const uint32_t layer_count,
	const uint32_t mip_count)
{
	ProcessedEnvironmentImage image{
		.width = width,
		.height = height,
		.layer_count = layer_count,
	};
	size_t total_size = 0;
	for (uint32_t mip = 0; mip < mip_count; ++mip)
	{
		const size_t size = mip_byte_size(
			std::max(1u, width >> mip),
			std::max(1u, height >> mip),
			layer_count);
		image.mip_sizes.push_back(size);
		if (total_size > std::numeric_limits<size_t>::max() - size)
			throw std::overflow_error("environment mip chain is too large");
		total_size += size;
	}
	image.rgba8_linear.resize(total_size);
	return image;
}

void write_pixel(
	ProcessedEnvironmentImage& image,
	const size_t offset,
	const glm::vec3 color)
{
	image.rgba8_linear[offset] = quantize(color.r);
	image.rgba8_linear[offset + 1] = quantize(color.g);
	image.rgba8_linear[offset + 2] = quantize(color.b);
	image.rgba8_linear[offset + 3] = std::byte{255};
}

uint32_t complete_mip_count(uint32_t size)
{
	uint32_t count = 1;
	while (size > 1)
	{
		size >>= 1;
		++count;
	}
	return count;
}

ProcessedEnvironmentImage generate_irradiance(
	const std::array<LinearFace, CUBEMAP_LAYER_COUNT>& faces,
	const EnvironmentMapProcessor::Settings& settings)
{
	auto image = make_image(
		settings.irradiance_size,
		settings.irradiance_size,
		CUBEMAP_LAYER_COUNT,
		1);
	const size_t layer_size = mip_byte_size(
		settings.irradiance_size, settings.irradiance_size, 1);
	for (uint32_t face = 0; face < CUBEMAP_LAYER_COUNT; ++face)
	{
		for (uint32_t y = 0; y < settings.irradiance_size; ++y)
		{
			for (uint32_t x = 0; x < settings.irradiance_size; ++x)
			{
				const glm::vec3 normal = cubemap_texel_direction(
					face, x, y, settings.irradiance_size);
				const TangentFrame frame = make_tangent_frame(normal);
				glm::vec3 irradiance(0.0f);
				for (uint32_t sample_index = 0;
					sample_index < settings.irradiance_sample_count;
					++sample_index)
				{
					irradiance += sample_cubemap(faces, cosine_hemisphere_sample(
						hammersley(sample_index, settings.irradiance_sample_count), frame));
				}
				irradiance /= static_cast<float>(settings.irradiance_sample_count);
				const size_t offset = face * layer_size
					+ (static_cast<size_t>(y) * settings.irradiance_size + x) * CHANNEL_COUNT;
				write_pixel(image, offset, irradiance);
			}
		}
	}
	return image;
}

ProcessedEnvironmentImage generate_prefiltered_specular(
	const std::array<LinearFace, CUBEMAP_LAYER_COUNT>& faces,
	const EnvironmentMapProcessor::Settings& settings)
{
	const uint32_t mip_count = complete_mip_count(settings.prefiltered_size);
	auto image = make_image(
		settings.prefiltered_size,
		settings.prefiltered_size,
		CUBEMAP_LAYER_COUNT,
		mip_count);
	size_t mip_offset = 0;
	for (uint32_t mip = 0; mip < mip_count; ++mip)
	{
		const uint32_t size = std::max(1u, settings.prefiltered_size >> mip);
		const size_t layer_size = mip_byte_size(size, size, 1);
		const float roughness = mip_count == 1
			? 0.0f
			: static_cast<float>(mip) / static_cast<float>(mip_count - 1);
		for (uint32_t face = 0; face < CUBEMAP_LAYER_COUNT; ++face)
		{
			for (uint32_t y = 0; y < size; ++y)
			{
				for (uint32_t x = 0; x < size; ++x)
				{
					const glm::vec3 reflection = cubemap_texel_direction(face, x, y, size);
					const TangentFrame frame = make_tangent_frame(reflection);
					glm::vec3 filtered(0.0f);
					float total_weight = 0.0f;
					const uint32_t sample_count = mip == 0
						? 1 : settings.prefiltered_sample_count;
					for (uint32_t sample_index = 0; sample_index < sample_count; ++sample_index)
					{
						const glm::vec3 half_vector = importance_sample_ggx(
							hammersley(sample_index, sample_count), frame, roughness);
						const glm::vec3 light_dir = glm::normalize(
							2.0f * glm::dot(reflection, half_vector) * half_vector - reflection);
						const float n_dot_l = std::max(glm::dot(reflection, light_dir), 0.0f);
						if (n_dot_l <= 0.0f)
							continue;
						filtered += sample_cubemap(faces, light_dir) * n_dot_l;
						total_weight += n_dot_l;
					}
					if (total_weight > 0.0f)
						filtered /= total_weight;
					const size_t offset = mip_offset + face * layer_size
						+ (static_cast<size_t>(y) * size + x) * CHANNEL_COUNT;
					write_pixel(image, offset, filtered);
				}
			}
		}
		mip_offset += image.mip_sizes[mip];
	}
	return image;
}

glm::vec2 integrate_brdf(
	const float n_dot_v,
	const float roughness,
	const uint32_t sample_count)
{
	const glm::vec3 normal(0.0f, 0.0f, 1.0f);
	const TangentFrame frame = make_tangent_frame(normal);
	const glm::vec3 view_dir(
		std::sqrt(std::max(0.0f, 1.0f - n_dot_v * n_dot_v)), 0.0f, n_dot_v);
	glm::vec2 result(0.0f);
	const float alpha = roughness * roughness;
	const float alpha_squared = alpha * alpha;
	for (uint32_t sample_index = 0; sample_index < sample_count; ++sample_index)
	{
		const glm::vec3 half_vector = importance_sample_ggx(
			hammersley(sample_index, sample_count), frame, roughness);
		const glm::vec3 light_dir = glm::normalize(
			2.0f * glm::dot(view_dir, half_vector) * half_vector - view_dir);
		const float n_dot_l = std::max(light_dir.z, 0.0f);
		const float n_dot_h = std::max(half_vector.z, 0.0f);
		const float v_dot_h = std::max(glm::dot(view_dir, half_vector), 0.0f);
		if (n_dot_l <= 0.0f || n_dot_h <= 0.0f)
			continue;

		const float ggx_view = n_dot_l * std::sqrt(
			n_dot_v * n_dot_v * (1.0f - alpha_squared) + alpha_squared);
		const float ggx_light = n_dot_v * std::sqrt(
			n_dot_l * n_dot_l * (1.0f - alpha_squared) + alpha_squared);
		const float visibility = 0.5f / std::max(ggx_view + ggx_light, 0.000001f);
		const float geometry_visibility = 4.0f * n_dot_l * visibility
			* v_dot_h / n_dot_h;
		const float fresnel = std::pow(1.0f - v_dot_h, 5.0f);
		result.x += (1.0f - fresnel) * geometry_visibility;
		result.y += fresnel * geometry_visibility;
	}
	return result / static_cast<float>(sample_count);
}

ProcessedEnvironmentImage generate_brdf_lut(
	const EnvironmentMapProcessor::Settings& settings)
{
	auto image = make_image(
		settings.brdf_lut_size,
		settings.brdf_lut_size,
		1,
		1);
	for (uint32_t y = 0; y < settings.brdf_lut_size; ++y)
	{
		const float roughness = (static_cast<float>(y) + 0.5f)
			/ static_cast<float>(settings.brdf_lut_size);
		for (uint32_t x = 0; x < settings.brdf_lut_size; ++x)
		{
			const float n_dot_v = (static_cast<float>(x) + 0.5f)
				/ static_cast<float>(settings.brdf_lut_size);
			const glm::vec2 value = integrate_brdf(
				n_dot_v, roughness, settings.brdf_sample_count);
			const size_t offset = (static_cast<size_t>(y) * settings.brdf_lut_size + x)
				* CHANNEL_COUNT;
			image.rgba8_linear[offset] = quantize(value.x);
			image.rgba8_linear[offset + 1] = quantize(value.y);
			image.rgba8_linear[offset + 2] = std::byte{0};
			image.rgba8_linear[offset + 3] = std::byte{255};
		}
	}
	return image;
}

void validate(
	const std::array<EnvironmentFace, CUBEMAP_LAYER_COUNT>& faces,
	const EnvironmentMapProcessor::Settings& settings)
{
	const auto& first = faces.front();
	if (first.width == 0 || first.height == 0 || first.width != first.height)
		throw std::invalid_argument("environment cubemap faces must be non-empty and square");
	for (const auto& face : faces)
	{
		if (face.width != first.width || face.height != first.height)
			throw std::invalid_argument("environment cubemap faces must have matching dimensions");
		if (face.rgba8_srgb.size() != mip_byte_size(face.width, face.height, 1))
			throw std::invalid_argument("environment cubemap face must contain one RGBA8 image");
	}
	if (settings.irradiance_size == 0 || settings.prefiltered_size == 0
		|| settings.brdf_lut_size == 0)
		throw std::invalid_argument("environment output dimensions must be non-zero");
	if (settings.irradiance_sample_count == 0 || settings.prefiltered_sample_count == 0
		|| settings.brdf_sample_count == 0)
		throw std::invalid_argument("environment sample counts must be non-zero");
}
}


ProcessedEnvironment EnvironmentMapProcessor::process(
	const std::array<EnvironmentFace, 6>& faces)
{
	return process(faces, Settings{});
}

ProcessedEnvironment EnvironmentMapProcessor::process(
	const std::array<EnvironmentFace, 6>& faces,
	const Settings& settings)
{
	validate(faces, settings);
	const auto linear_faces = linearize_faces(faces);
	return {
		.irradiance = generate_irradiance(linear_faces, settings),
		.prefiltered_specular = generate_prefiltered_specular(linear_faces, settings),
		.brdf_lut = generate_brdf_lut(settings),
	};
}
