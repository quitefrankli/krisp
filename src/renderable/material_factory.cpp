#include "material_factory.hpp"
#include <array>


std::unique_ptr<Material> MaterialFactory::fetch_preset(EMaterialPreset preset)
{
	glm::vec4 base_color(1.0f);
	float metallic = 0.0f;
	float roughness = 0.5f;
	switch (preset)
	{
		case EMaterialPreset::ALWAYS_LIT:
		case EMaterialPreset::LIGHT_SOURCE:
			base_color = glm::vec4(1.0f, 1.0f, 0.0f, 1.0f);
			break;
		case EMaterialPreset::RUBBER:
			base_color = glm::vec4(0.5f, 0.5f, 0.4f, 1.0f);
			roughness = 0.9f;
			break;
		case EMaterialPreset::PLASTIC:
			base_color = glm::vec4(0.55f, 0.55f, 0.55f, 1.0f);
			roughness = 0.4f;
			break;
		case EMaterialPreset::METAL:
			base_color = glm::vec4(0.5f, 0.5f, 0.5f, 1.0f);
			metallic = 1.0f;
			roughness = 0.25f;
			break;
		case EMaterialPreset::DIFFUSE:
			roughness = 1.0f;
			break;
		case EMaterialPreset::GIZMO_ARROW:
		case EMaterialPreset::GIZMO_X_AXIS:
			base_color = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
			break;
		case EMaterialPreset::GIZMO_ARC:
			base_color = glm::vec4(0.0f, 1.0f, 1.0f, 1.0f);
			break;
		case EMaterialPreset::GIZMO_Y_AXIS:
			base_color = glm::vec4(0.0f, 1.0f, 0.0f, 1.0f);
			break;
		case EMaterialPreset::GIZMO_Z_AXIS:
			base_color = glm::vec4(0.0f, 0.45f, 1.0f, 1.0f);
			break;
		case EMaterialPreset::GIZMO_UNIFORM_SCALE:
			base_color = glm::vec4(1.0f, 0.85f, 0.0f, 1.0f);
			break;
		case EMaterialPreset::DEFAULT:
		default:
			metallic = 1.0f;
			roughness = 1.0f;
			break;
	}

	return std::make_unique<PbrMaterial>(base_color, metallic, roughness);
}

std::unique_ptr<Material> MaterialFactory::fetch_white_texture()
{
	struct WhiteTextureData : TextureData
	{
		std::array<std::byte, 4> pixels{
			std::byte{255}, std::byte{255}, std::byte{255}, std::byte{255} };
		std::byte* get() override { return pixels.data(); }
	};

	auto material = std::make_unique<TextureMaterial>();
	material->data = std::make_unique<WhiteTextureData>();
	material->data_len = 4;
	material->width = 1;
	material->height = 1;
	material->channels = 4;
	material->mip_sizes = { 4 };
	material->source = "(none)";
	return material;
}
