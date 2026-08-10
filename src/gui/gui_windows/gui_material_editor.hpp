#pragma once

#include "gui_windows.hpp"
#include "renderable/material.hpp"

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <array>


class GuiMaterialEditor : public EngineUiWindow
{
public:
	GuiMaterialEditor();
	void process(GameEngine& engine) override;
	void draw() override;

private:
	struct MaterialChange
	{
		struct TextureChange
		{
			bool changed = false;
			std::optional<std::string> replacement;
		};

		RenderableID renderable_id;
		glm::vec4 base_color_factor{ 1.0f };
		float metallic_factor = 1.0f;
		float roughness_factor = 1.0f;
		float normal_scale = 1.0f;
		EAlphaMode alpha_mode = EAlphaMode::OPAQUE;
		float alpha_cutoff = 0.5f;
		bool double_sided = false;
		glm::vec3 emissive_factor{ 0.0f };
		std::array<TextureChange, 4> textures;
	};

	std::vector<std::string> renderable_labels;
	std::vector<RenderableID> renderable_ids;
	GuiVar<int> selected_renderable = 0;
	std::optional<ObjectID> target_object;
	std::optional<MaterialChange> pending_change;
	std::optional<RenderableID> loaded_renderable;
	std::optional<std::string> load_error;
	std::string target_status = "Select an object";
	glm::vec4 base_color_factor{ 1.0f };
	float metallic_factor = 1.0f;
	float roughness_factor = 1.0f;
	float normal_scale = 1.0f;
	EAlphaMode alpha_mode = EAlphaMode::OPAQUE;
	float alpha_cutoff = 0.5f;
	bool double_sided = false;
	glm::vec3 emissive_factor{ 0.0f };
	std::array<std::array<char, 256>, 4> texture_names{};
	std::array<std::string, 4> loaded_texture_names;
	bool compatible = false;
	bool texture_compatible = false;
};
