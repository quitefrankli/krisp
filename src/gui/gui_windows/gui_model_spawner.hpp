#pragma once

#include "gui_windows.hpp"
#include "gui_window_helpers.hpp"

class GuiModelSpawner : public EngineUiWindow
{
public:
	GuiModelSpawner();

	void process(GameEngine& engine) override;
	void draw() override;

private:
	void refresh_models();

	std::vector<std::string> model_paths;
	GuiWindowDetail::ResourceTree model_tree;
	GuiVar<int> selected_model = 0;
	GuiVar<bool> merge_imported_meshes = false;
	std::optional<std::string> model_to_spawn;
	bool should_refresh_models = false;
	bool model_dropdown_open = false;
	std::optional<std::string> load_error;
};
