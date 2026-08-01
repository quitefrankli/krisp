#include "persistent_gui_windows.hpp"

#include "game_engine.hpp"
#include "graphics_engine/engine_base.hpp"

#include <imgui.h>

GuiFPSCounter::GuiFPSCounter() :
	PersistentUiWindow({ "fps_counter", "FPS Counter", GuiPanelDock::NONE, true, false })
{
}

void GuiFPSCounter::process(GameEngine& engine)
{
	tps = engine.get_tps();
	fps = engine.get_graphics_engine().get_fps();
}

void GuiFPSCounter::draw()
{
	float current_fps = 0.0f;
	float current_tps = 0.0f;
	current_fps = fps;
	current_tps = tps;
	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(
		ImVec2(
			viewport->WorkPos.x + viewport->WorkSize.x - 12.0f,
			viewport->WorkPos.y + viewport->WorkSize.y - 12.0f),
		ImGuiCond_Always, ImVec2(1.0f, 1.0f));
	ImGui::SetNextWindowSize(ImVec2(190.0f, 104.0f), ImGuiCond_Always);
	ImGui::SetNextWindowBgAlpha(0.90f);
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.025f, 0.035f, 0.055f, 0.90f));
	ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.20f, 0.55f, 0.90f, 0.90f));
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.96f, 0.98f, 1.0f, 1.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 7.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 10.0f));
	ImGui::Begin(get_imgui_name(), nullptr,
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoInputs |
		ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoDocking |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoResize);
	ImGui::SetWindowFontScale(1.45f);
	ImGui::Text("FPS  %.1f", current_fps);
	ImGui::Text("TPS  %.1f", current_tps);
	ImGui::End();
	ImGui::PopStyleVar(3);
	ImGui::PopStyleColor(3);
}
