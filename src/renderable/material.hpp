#pragma once

#include "renderable/render_types.hpp"
#include "shared_data_structures.hpp"
#include "identifications.hpp"

#include <glm/vec4.hpp>

#include <cmath>
#include <stdexcept>
#include <string_view>
#include <string>
#include <memory>
#include <optional>
#include <vector>


struct Material
{
public:
	virtual ~Material() = default;
	MaterialID get_id() const { return id; }

private:
	const MaterialID id = MaterialID::generate_new_id();
};

struct PbrMaterial : public Material
{
public:
	PbrMaterial() : PbrMaterial(glm::vec4(1.0f), 1.0f, 1.0f) {}

	enum class TextureSampler
	{
		REPEAT,
		CLAMP_TO_EDGE,
	};

	struct TextureBinding
	{
		MaterialID texture;
		TextureSampler sampler = TextureSampler::REPEAT;

		auto operator<=>(const TextureBinding&) const = default;
	};

	struct TextureSlots
	{
		std::optional<TextureBinding> base_color;
		std::optional<TextureBinding> metallic_roughness;
		std::optional<TextureBinding> normal;
	};

	PbrMaterial(
		const glm::vec4 base_color_factor,
		const float metallic_factor,
		const float roughness_factor,
		TextureSlots textures = {},
		const float normal_scale = 1.0f)
	{
		const auto valid_factor = [](const float factor)
		{
			return std::isfinite(factor) && factor >= 0.0f && factor <= 1.0f;
		};
		if (!valid_factor(base_color_factor.r) || !valid_factor(base_color_factor.g)
			|| !valid_factor(base_color_factor.b) || !valid_factor(base_color_factor.a)
			|| !valid_factor(metallic_factor) || !valid_factor(roughness_factor))
			throw std::invalid_argument("PbrMaterial factors must be finite and in [0, 1]");
		if (!std::isfinite(normal_scale))
			throw std::invalid_argument("PbrMaterial normal scale must be finite");

		data.base_color_factor = base_color_factor;
		data.metallic_factor = metallic_factor;
		data.roughness_factor = roughness_factor;
		data.normal_scale = normal_scale;
		data.texture_flags = (textures.base_color ? SDS::PBR_BASE_COLOR_TEXTURE : 0)
			| (textures.metallic_roughness ? SDS::PBR_METALLIC_ROUGHNESS_TEXTURE : 0)
			| (textures.normal ? SDS::PBR_NORMAL_TEXTURE : 0);
		this->textures = std::move(textures);
	}

	bool has_textures() const
	{
		return textures.base_color || textures.metallic_roughness || textures.normal;
	}

	SDS::MaterialData data{};
	TextureSlots textures;
};

struct TextureData
{
	virtual ~TextureData() = default;
	virtual std::byte* get() = 0;
};

struct OwnedTextureData final : TextureData
{
	explicit OwnedTextureData(std::vector<std::byte> bytes) : bytes(std::move(bytes)) {}
	std::byte* get() override { return bytes.data(); }

private:
	std::vector<std::byte> bytes;
};

enum class ETextureSemantic
{
	BASE_COLOR,
	METALLIC_ROUGHNESS,
	NORMAL,
	COUNT
};

enum class ETextureFormat
{
	RGBA8,
	BC3,
};

struct SampledMaterial : public Material
{
	ETextureSemantic semantic = ETextureSemantic::BASE_COLOR;
	uint32_t width = 0;
	uint32_t height = 0;
	virtual bool is_premultiplied() const { return false; }
};

struct TextureMaterial : public SampledMaterial
{
	std::unique_ptr<TextureData> data;
	size_t data_len = 0;
	uint32_t channels = 4; 
	ETextureFormat format = ETextureFormat::RGBA8;
	std::vector<size_t> mip_sizes;
	std::string source;
	uint32_t texture_id = 0;
};
