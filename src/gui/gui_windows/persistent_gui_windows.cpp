#include "persistent_gui_windows.hpp"

#include "game_engine.hpp"
#include "graphics_engine/engine_base.hpp"
#include "command_registry.hpp"

#include <imgui.h>

#include <utility>

GuiFPSCounter::GuiFPSCounter() :
	PersistentUiWindow({ "fps_counter", "FPS Counter", GuiPanelDock::NONE, false, false })
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
			viewport->WorkPos.y + 12.0f),
		ImGuiCond_Always, ImVec2(1.0f, 0.0f));
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

GuiCommandPrompt::GuiCommandPrompt() :
	PersistentUiWindow({ "command_prompt", "Command Prompt", GuiPanelDock::NONE, false, false })
{
}

void GuiCommandPrompt::open()
{
	set_visible(true);
	input_buffer.fill('\0');
	focus_input = true;
	just_opened = true;
}

void GuiCommandPrompt::process(GameEngine& engine)
{
	if (!is_open()) return;
	auto entries = engine.get_commands().matches("");
	suggestions.clear();
	suggestions.reserve(entries.size());
	for (auto& entry : entries)
		suggestions.push_back({ std::move(entry.name), std::move(entry.description) });
}

std::optional<std::string> GuiCommandPrompt::take_submission()
{
	return std::exchange(pending_submission, std::nullopt);
}

int GuiCommandPrompt::input_callback(ImGuiInputTextCallbackData* data)
{
	auto& prompt = *static_cast<GuiCommandPrompt*>(data->UserData);
	if (data->EventFlag == ImGuiInputTextFlags_CallbackCharFilter)
		return data->EventChar == '/' ? 1 : 0;

	if (data->EventFlag == ImGuiInputTextFlags_CallbackCompletion && !prompt.suggestions.empty())
	{
		const std::string_view input(data->Buf, static_cast<size_t>(data->BufTextLen));
		if (input.find_first_of(" \t\r\n") != std::string_view::npos)
			return 0;
		const auto prefix = input;
		std::vector<const Suggestion*> matches;
		for (const auto& suggestion : prompt.suggestions)
			if (suggestion.name.starts_with(prefix))
				matches.push_back(&suggestion);
		if (!matches.empty())
		{
			std::string common_prefix = matches.front()->name;
			for (size_t index = 1; index < matches.size(); ++index)
			{
				const auto* match = matches[index];
				size_t length = 0;
				while (length < common_prefix.size() && length < match->name.size() &&
					common_prefix[length] == match->name[length])
					++length;
				common_prefix.resize(length);
			}
			std::string completion = common_prefix;
			if (matches.size() == 1 && matches.front()->name == common_prefix)
				completion += ' ';
			data->DeleteChars(0, data->BufTextLen);
			data->InsertChars(0, completion.c_str());
		}
	}
	return 0;
}

void GuiCommandPrompt::draw()
{
	if (!is_open())
		return;

	const auto* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x, viewport->Pos.y + viewport->Size.y),
		ImGuiCond_Always, ImVec2(0.0f, 1.0f));
	ImGui::SetNextWindowSize(ImVec2(viewport->Size.x, viewport->Size.y * 0.5f), ImGuiCond_Always);
	ImGui::SetNextWindowViewport(viewport->ID);
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.02f, 0.025f, 0.03f, 0.82f));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	const auto window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking;
	if (ImGui::Begin(get_imgui_name(), nullptr, window_flags))
	{
		// Leave room below the scrollable feedback for the fixed input line.
		const float footer_height = ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y;
		ImGui::BeginChild("feedback", ImVec2(0.0f, -footer_height));
		if (!output.empty())
			ImGui::TextWrapped("%s", output.c_str());

		const std::string_view input(input_buffer.data());
		const auto first_space = input.find_first_of(" \t\r\n");
		const auto prefix = input;
		if (first_space == std::string_view::npos)
		{
			for (const auto& suggestion : suggestions)
			{
				if (!suggestion.name.starts_with(prefix))
					continue;
				ImGui::TextUnformatted(suggestion.name.c_str());
				ImGui::SameLine(130.0f);
				ImGui::TextDisabled("%s", suggestion.description.c_str());
			}
		}
		ImGui::EndChild();
		ImGui::Separator();
		ImGui::TextUnformatted(">");
		ImGui::SameLine();
		ImGui::SetNextItemWidth(-1.0f);
		if (focus_input)
		{
			ImGui::SetKeyboardFocusHere();
			focus_input = false;
		}

		const auto flags = ImGuiInputTextFlags_EnterReturnsTrue |
			ImGuiInputTextFlags_CallbackCompletion |
			ImGuiInputTextFlags_CallbackCharFilter;
		const bool submitted = ImGui::InputText("##command", input_buffer.data(), input_buffer.size(),
			flags, input_callback, this);
		if (submitted)
		{
			pending_submission = input_buffer.data();
			input_buffer.fill('\0');
			focus_input = true;
		}

		if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
			(ImGui::IsKeyPressed(ImGuiKey_Escape, false)
				|| (!just_opened && ImGui::IsKeyPressed(ImGuiKey_Slash, false))))
			set_visible(false);
	}
	just_opened = false;
	ImGui::End();
	ImGui::PopStyleVar(2);
	ImGui::PopStyleColor();
}
