#include "application_ui_manager.hpp"
#include "gui_manager.hpp"
#include "game_engine.hpp"

#include <imgui.h>

void ApplicationUiElement::draw()
{
	if (begin(window_flags(), false))
		draw_contents();
	end();
}

int ApplicationUiWindow::window_flags() const
{
	return ImGuiWindowFlags_NoDocking |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoSavedSettings;
}

int ApplicationUiOverlay::window_flags() const
{
	return ImGuiWindowFlags_NoDocking |
		ImGuiWindowFlags_NoDecoration |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoNav |
		ImGuiWindowFlags_NoInputs;
}

void EngineUiManager::process_persistent(GameEngine& engine)
{
	std::optional<std::string> submission;
	{
		const std::lock_guard lock(state_mutex);
		for (auto& gui : persistent_windows)
			gui->process(engine);
		submission = command_prompt.take_submission();
	}
	if (submission)
	{
		auto output = engine.get_commands().execute(engine, *submission);
		const std::lock_guard lock(state_mutex);
		command_prompt.set_output(std::move(output));
	}
}
