#pragma once

#include "gui_windows.hpp"

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

	void refresh_textures();
	void draw_texture_section(
		const char* title,
		ETextureSemantic semantic,
		const std::string& current_label,
		bool& dropdown_was_open);

	std::vector<std::string> texture_paths;
	std::vector<std::string> texture_names;
	std::vector<std::string> renderable_labels;
	std::vector<RenderableID> renderable_ids;
	GuiVar<int> selected_renderable = 0;
	std::optional<ObjectID> target_object;
	std::optional<TextureChange> pending_change;
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
	// process() runs on the game thread while draw() runs on the graphics thread.
	std::mutex state_mutex;
};
