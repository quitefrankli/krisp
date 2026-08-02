#include "gui_window_helpers.hpp"

#include "resource_loader/resource_loader.hpp"
#include "utility.hpp"

#include <imgui.h>
#include <imgui_internal.h>
#include <fmt/core.h>
#include <quill/LogMacros.h>

#include <algorithm>
#include <cctype>
#include <filesystem>

namespace GuiWindowDetail
{
namespace
{
bool case_insensitive_less(const std::string& lhs, const std::string& rhs)
{
	const bool precedes = std::lexicographical_compare(
		lhs.begin(), lhs.end(), rhs.begin(), rhs.end(), [](const unsigned char left, const unsigned char right) {
			return std::tolower(left) < std::tolower(right);
		});
	const bool follows = std::lexicographical_compare(
		rhs.begin(), rhs.end(), lhs.begin(), lhs.end(), [](const unsigned char left, const unsigned char right) {
			return std::tolower(left) < std::tolower(right);
		});
	return precedes || (!follows && lhs < rhs);
}

void sort_resource_tree(ResourceTree& tree)
{
	std::ranges::sort(tree, [](const ResourceTreeNode& lhs, const ResourceTreeNode& rhs) {
		const bool lhs_directory = !lhs.resource_index;
		const bool rhs_directory = !rhs.resource_index;
		if (lhs_directory != rhs_directory)
			return lhs_directory;
		return case_insensitive_less(lhs.name, rhs.name);
	});
	for (auto& node : tree)
		sort_resource_tree(node.children);
}

bool contains_selection(const ResourceTreeNode& node, const std::optional<size_t> selected)
{
	if (node.resource_index == selected && selected)
		return true;
	return std::ranges::any_of(node.children, [&](const ResourceTreeNode& child) {
		return contains_selection(child, selected);
	});
}

std::optional<size_t> draw_resource_tree_nodes(
	const ResourceTree& tree,
	const std::vector<std::string>& paths,
	const std::optional<size_t> selected)
{
	std::optional<size_t> result;
	for (const auto& node : tree)
	{
		ImGui::PushID(node.name.c_str());
		if (!node.resource_index)
		{
			if (contains_selection(node, selected))
				ImGui::SetNextItemOpen(true, ImGuiCond_Once);
			if (ImGui::TreeNodeEx("##directory", ImGuiTreeNodeFlags_SpanAvailWidth, "%s", node.name.c_str()))
			{
				result = draw_resource_tree_nodes(node.children, paths, selected);
				ImGui::TreePop();
			}
		}
		else
		{
			const size_t index = *node.resource_index;
			if (ImGui::Selectable(node.name.c_str(), selected == index))
				result = index;
			if (index < paths.size() && ImGui::IsItemHovered())
				ImGui::SetTooltip("%s", paths[index].c_str());
		}
		ImGui::PopID();
		if (result)
			break;
	}
	return result;
}
}

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

ResourceTree build_resource_tree(const std::vector<std::string>& paths)
{
	ResourceTree tree;
	for (size_t index = 0; index < paths.size(); ++index)
	{
		std::vector<std::string> components;
		for (const auto& component : std::filesystem::path(paths[index]))
			if (const auto name = component.generic_string(); !name.empty() && name != ".")
				components.push_back(name);
		if (components.empty())
			continue;

		ResourceTree* level = &tree;
		for (size_t component_index = 0; component_index < components.size(); ++component_index)
		{
			const bool file = component_index + 1 == components.size();
			if (file)
			{
				level->push_back({ components[component_index], index, {} });
				continue;
			}
			auto directory = std::ranges::find_if(*level, [&](const ResourceTreeNode& node) {
				return !node.resource_index && node.name == components[component_index];
			});
			if (directory == level->end())
			{
				level->push_back({ components[component_index], std::nullopt, {} });
				directory = std::prev(level->end());
			}
			level = &directory->children;
		}
	}
	sort_resource_tree(tree);
	return tree;
}

std::optional<size_t> draw_resource_tree(
	const ResourceTree& tree,
	const std::vector<std::string>& paths,
	const std::optional<size_t> selected)
{
	return draw_resource_tree_nodes(tree, paths, selected);
}
}
