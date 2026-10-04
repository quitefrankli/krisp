#pragma once

#include "gui_windows.hpp"
#include "renderable/material.hpp"
#include "renderable/composited_texture_material.hpp"

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
	struct OverlayChange
	{
		RenderableID renderable_id;
		TextureCompositionOverlay overlay;
	};

	std::vector<std::string> renderable_labels;
	std::vector<RenderableID> renderable_ids;
	GuiVar<int> selected_renderable = 0;
	std::optional<ObjectID> target_object;
	std::optional<MaterialChange> pending_change;
	std::optional<OverlayChange> pending_overlay;
	std::optional<RenderableID> loaded_renderable;
	std::optional<std::string> load_error;
	std::string target_status = "Select an object";
	std::vector<std::string> texture_paths;
	GuiWindowDetail::ResourceTree texture_tree;
	glm::vec4 base_color_factor{ 1.0f };
	float metallic_factor = 1.0f;
	float roughness_factor = 1.0f;
	float normal_scale = 1.0f;
	EAlphaMode alpha_mode = EAlphaMode::OPAQUE;
	float alpha_cutoff = 0.5f;
	bool double_sided = false;
	glm::vec3 emissive_factor{ 0.0f };
	std::array<std::string, 4> texture_names;
	std::array<std::string, 4> loaded_texture_names;
	std::array<bool, 4> texture_dropdown_open{};
	TextureCompositionOverlay overlay_draft;
	size_t composition_overlay_count = 0;
	bool overlay_dropdown_open = false;
	bool should_refresh_textures = false;
	bool compatible = false;
	bool texture_compatible = false;
	bool base_color_texture_available = false;

	void reset_overlay_draft();
	void draw_texture_picker(std::string& name, bool& was_open);
};
