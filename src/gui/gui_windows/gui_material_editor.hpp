#pragma once

#include "gui_windows.hpp"
#include "renderable/composited_texture_material.hpp"

enum class ETextureSemantic;

class GuiMaterialEditor : public EngineUiWindow
{
public:
	GuiMaterialEditor();
	void process(GameEngine& engine) override;
	void draw() override;

private:
	struct TextureChange
	{
		RenderableID renderable_id;
		ETextureSemantic semantic;
		std::optional<std::string> path;
		bool matte = false;
	};
	struct OverlayChange
	{
		RenderableID renderable_id;
		TextureCompositionOverlay overlay;
	};

	void refresh_textures();
	void reset_overlay_draft();
	void draw_texture_section(
		const char* title,
		ETextureSemantic semantic,
		const std::string& current_label,
		bool& dropdown_was_open);
	void draw_overlay_section();

	std::vector<std::string> texture_paths;
	std::vector<std::string> texture_names;
	std::vector<std::string> renderable_labels;
	std::vector<RenderableID> renderable_ids;
	GuiVar<int> selected_renderable = 0;
	std::optional<ObjectID> target_object;
	std::optional<TextureChange> pending_change;
	std::optional<OverlayChange> pending_overlay;
	std::optional<RenderableID> overlay_draft_target;
	TextureCompositionOverlay overlay_draft;
	std::optional<std::string> load_error;
	std::string target_status = "Select an object";
	std::string diffuse_label = "(none)";
	std::string normal_label = "(none)";
	std::string specular_label = "(glossy)";
	bool compatible = false;
	bool should_refresh_textures = false;
	bool diffuse_dropdown_open = false;
	bool normal_dropdown_open = false;
	bool specular_dropdown_open = false;
	bool overlay_dropdown_open = false;
};
