#include "gui_window_helpers.hpp"

#include "resource_loader/resource_loader.hpp"
#include "utility.hpp"

#include <imgui.h>
#include <imgui_internal.h>
#include <fmt/core.h>
#include <quill/LogMacros.h>

#include <algorithm>

namespace GuiWindowDetail
{
std::string report_resource_load_error(
	const std::string_view context,
	const ResourceLoadError& error)
{
	std::string message = fmt::format("{}: {}", context, error.what());
	LOG_ERROR(Utility::get_logger(), "{}", message);
	return message;
}

void draw_resource_load_error(const std::optional<std::string>& error)
{
	if (!error)
		return;

	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.25f, 0.25f, 1.0f));
	ImGui::TextWrapped("%s", error->c_str());
	ImGui::PopStyleColor();
}

bool begin_italic_combo(const char* label, const char* preview)
{
	ImDrawList* draw_list = ImGui::GetWindowDrawList();
	const int first_vertex = draw_list->VtxBuffer.Size;
	const bool open = ImGui::BeginCombo(label, preview);
	const ImVec2 white_pixel = ImGui::GetIO().Fonts->TexUvWhitePixel;

	float baseline = 0.0f;
	for (int i = first_vertex; i < draw_list->VtxBuffer.Size; ++i)
	{
		const auto& vertex = draw_list->VtxBuffer[i];
		if (vertex.uv.x != white_pixel.x || vertex.uv.y != white_pixel.y)
			baseline = std::max(baseline, vertex.pos.y);
	}
	for (int i = first_vertex; i < draw_list->VtxBuffer.Size; ++i)
	{
		auto& vertex = draw_list->VtxBuffer[i];
		if (vertex.uv.x != white_pixel.x || vertex.uv.y != white_pixel.y)
			vertex.pos.x += (baseline - vertex.pos.y) * 0.18f;
	}

	return open;
}
}
