#include "application_ui_manager.hpp"

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
