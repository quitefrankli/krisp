#pragma once

#include "gui_windows.hpp"
#include "persistent_gui_windows.hpp"

#include <vector>
#include <memory>
#include <type_traits>
#include <unordered_map>


// Owns the editor-facing panels.  It deliberately has no ImGui backend state;
// GraphicsEngineGuiManager is the sole owner of the ImGui context.
class EngineUiManager
{
protected:
	std::vector<std::unique_ptr<GuiWindow>> gui_windows; 
	std::vector<std::unique_ptr<GuiWindow>> persistent_windows;
	std::unordered_map<std::string, bool> saved_panel_visibility;

public:
	EngineUiManager() :
		save_manager(spawn_gui<GuiSaveManager>()),
		graphic_settings(spawn_gui<GuiGraphicsSettings>()),
		object_spawner(spawn_gui<GuiObjectSpawner>()),
		model_spawner(spawn_gui<GuiModelSpawner>()),
		fps_counter(spawn_persistent_gui<GuiFPSCounter>()),
		statistics(spawn_gui<GuiStatistics>()),
		debug(spawn_gui<GuiDebug>()),
		photo(spawn_gui<GuiPhoto>()),
		render_slicer(spawn_gui<GuiRenderSlicer>()),
		animation_selector(spawn_gui<GuiAnimationSelector>()),
		material_editor(spawn_gui<GuiMaterialEditor>())
	{
	}

	template<typename Gui_T, typename... Args>
	Gui_T& spawn_gui(Args&&... args)
	{
		static_assert(std::is_base_of_v<EngineUiWindow, Gui_T>);
		gui_windows.push_back(std::make_unique<Gui_T>(std::forward<Args>(args)...));
		auto& gui = gui_windows.back();
		const auto saved = saved_panel_visibility.find(gui->get_panel_info().id);
		if (saved != saved_panel_visibility.end())
			gui->restore_visibility(saved->second);
		return *static_cast<Gui_T*>(gui.get());
	}

	template<typename Gui_T, typename... Args>
	Gui_T& spawn_persistent_gui(Args&&... args)
	{
		static_assert(std::is_base_of_v<PersistentUiWindow, Gui_T>);
		persistent_windows.push_back(
			std::make_unique<Gui_T>(std::forward<Args>(args)...));
		return *static_cast<Gui_T*>(persistent_windows.back().get());
	}

	void clear_saved_panel_visibility() { saved_panel_visibility.clear(); }
	bool& get_or_create_saved_panel_visibility(const std::string& id)
	{
		return saved_panel_visibility[id];
	}
	void apply_saved_panel_visibility()
	{
		for (auto& gui : gui_windows)
		{
			const auto saved = saved_panel_visibility.find(gui->get_panel_info().id);
			if (saved != saved_panel_visibility.end())
				gui->restore_visibility(saved->second);
		}
	}
	void reset_panel_visibility()
	{
		saved_panel_visibility.clear();
		for (auto& gui : gui_windows)
			gui->reset_visibility();
	}
	const std::vector<std::unique_ptr<GuiWindow>>& get_gui_windows() const { return gui_windows; }
	const std::vector<std::unique_ptr<GuiWindow>>& get_persistent_windows() const
	{
		return persistent_windows;
	}

	void update_buffer_capacities(const std::vector<std::pair<size_t, size_t>>& capacities)
	{
		statistics.update_buffer_capacities(capacities);
	}
	bool handle_key_input(const KeyInput& input)
	{
		return animation_selector.handle_key_input(input);
	}

	// references the EngineUiManager::gui_windows
	GuiSaveManager& save_manager;
	GuiGraphicsSettings& graphic_settings;
	GuiObjectSpawner& object_spawner;
	GuiModelSpawner& model_spawner;
	GuiFPSCounter& fps_counter;
	GuiStatistics& statistics;
	GuiDebug& debug;
	GuiPhoto& photo;
	GuiRenderSlicer& render_slicer;
	GuiAnimationSelector& animation_selector;
	GuiMaterialEditor& material_editor;

public: // for GameEngine
	void process(GameEngine& engine)
	{
		for (auto& gui : gui_windows)
		{
			gui->process(engine);
		}
	}
	void process_persistent(GameEngine& engine)
	{
		for (auto& gui : persistent_windows)
			gui->process(engine);
	}
};
