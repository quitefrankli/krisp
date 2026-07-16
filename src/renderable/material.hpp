#pragma once

#include "renderable/render_types.hpp"
#include "shared_data_structures.hpp"
#include "identifications.hpp"

#include <glm/vec3.hpp>

#include <string_view>
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

struct ColorMaterial : public Material
{
public:
	ColorMaterial()
	{
		static const glm::vec3 white = glm::vec3(1.0f);
		static const glm::vec3 black = glm::vec3(0.0f);
		data.ambient = white;
		data.diffuse = white;
		data.specular = white;
		data.emissive = black;
		data.shininess = 32.0f;
	}

	SDS::MaterialData data;
};

struct TextureData
{
	virtual ~TextureData() = default;
	virtual std::byte* get() = 0;
};

enum class ETextureSemantic
{
	BASE_COLOR,
	NORMAL
};

enum class ETextureFormat
{
	RGBA8,
	BC3,
};

struct TextureMaterial : public Material
{
	std::unique_ptr<TextureData> data;
	size_t data_len = 0;
	uint32_t width = 0;
	uint32_t height = 0;
	uint32_t channels = 4; 
	ETextureFormat format = ETextureFormat::RGBA8;
	std::vector<size_t> mip_sizes;
	std::string source;
	uint32_t texture_id = 0;
	ETextureSemantic semantic = ETextureSemantic::BASE_COLOR;
};
