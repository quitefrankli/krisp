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
	PbrMaterial(
		const glm::vec4 base_color_factor,
		const float metallic_factor,
		const float roughness_factor)
	{
		const auto valid_factor = [](const float factor)
		{
			return std::isfinite(factor) && factor >= 0.0f && factor <= 1.0f;
		};
		if (!valid_factor(base_color_factor.r) || !valid_factor(base_color_factor.g)
			|| !valid_factor(base_color_factor.b) || !valid_factor(base_color_factor.a)
			|| !valid_factor(metallic_factor) || !valid_factor(roughness_factor))
			throw std::invalid_argument("PbrMaterial factors must be finite and in [0, 1]");

		data.base_color_factor = base_color_factor;
		data.metallic_factor = metallic_factor;
		data.roughness_factor = roughness_factor;
	}

	SDS::MaterialData data{};
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
