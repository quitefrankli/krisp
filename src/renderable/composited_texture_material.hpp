#pragma once

#include "constants.hpp"
#include "entity_component_system/material_system.hpp"

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>


struct TextureCompositionOverlay
{
	std::string texture_filename;
	glm::vec2 centre{ 0.5f, 0.5f };
	glm::vec2 scale{ 1.0f, 1.0f };
	float rotation_radians = 0.0f;
	glm::vec3 tint{ 1.0f };
	float opacity = 1.0f;
};


struct TextureCompositionLayer
{
	MaterialHandle source;
	glm::vec2 centre{ 0.5f, 0.5f };
	glm::vec2 scale{ 1.0f, 1.0f };
	float rotation_radians = 0.0f;
	glm::vec3 tint{ 1.0f };
	float opacity = 1.0f;
};

struct CompositedTextureMaterial final : public SampledMaterial
{
	CompositedTextureMaterial(
		const uint32_t width_,
		const uint32_t height_,
		std::vector<TextureCompositionLayer> layers_) :
		layers(std::move(layers_))
	{
		width = width_;
		height = height_;
		semantic = ETextureSemantic::BASE_COLOR;
		if (width == 0 || height == 0)
			throw std::invalid_argument("CompositedTextureMaterial: output dimensions must be non-zero");
		if (layers.empty())
			throw std::invalid_argument("CompositedTextureMaterial: at least one layer is required");
		if (layers.size() > CSTS::MAX_TEXTURE_COMPOSITION_LAYERS)
			throw std::invalid_argument(
				"CompositedTextureMaterial: composition supports at most "
				+ std::to_string(CSTS::MAX_TEXTURE_COMPOSITION_LAYERS) + " total layers");
		for (const auto& layer : layers)
		{
			if (!layer.source)
				throw std::invalid_argument("CompositedTextureMaterial: layer source is empty");
			const auto* texture = dynamic_cast<const TextureMaterial*>(&layer.source->get());
			if (!texture)
				throw std::invalid_argument("CompositedTextureMaterial: nested compositions are unsupported");
			if (texture->semantic != ETextureSemantic::BASE_COLOR)
				throw std::invalid_argument("CompositedTextureMaterial: layer source is not base colour");
			const auto finite_vec2 = [](const glm::vec2& value) {
				return std::isfinite(value.x) && std::isfinite(value.y);
			};
			const auto unit_vec3 = [](const glm::vec3& value) {
				return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z)
					&& value.x >= 0.0f && value.x <= 1.0f
					&& value.y >= 0.0f && value.y <= 1.0f
					&& value.z >= 0.0f && value.z <= 1.0f;
			};
			if (!finite_vec2(layer.centre) || !finite_vec2(layer.scale)
				|| layer.scale.x <= 0.0f || layer.scale.y <= 0.0f
				|| !std::isfinite(layer.rotation_radians) || !unit_vec3(layer.tint)
				|| !std::isfinite(layer.opacity) || layer.opacity < 0.0f || layer.opacity > 1.0f)
				throw std::invalid_argument("CompositedTextureMaterial: invalid layer parameters");
		}
		const auto& bottom = dynamic_cast<const TextureMaterial&>(layers.front().source->get());
		if (bottom.width != width || bottom.height != height)
			throw std::invalid_argument(
				"CompositedTextureMaterial: output dimensions must match the bottom layer");
	}

	bool is_premultiplied() const override { return true; }

	std::vector<TextureCompositionLayer> layers;
};
