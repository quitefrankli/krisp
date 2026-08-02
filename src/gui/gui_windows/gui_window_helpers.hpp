#pragma once

#include <optional>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

class ResourceLoadError;

namespace GuiWindowDetail
{
struct ResourceTreeNode
{
	std::string name;
	std::optional<size_t> resource_index;
	std::vector<ResourceTreeNode> children;
};

using ResourceTree = std::vector<ResourceTreeNode>;

std::string report_resource_load_error(
	std::string_view context,
	const ResourceLoadError& error);
void draw_resource_load_error(const std::optional<std::string>& error);
bool begin_italic_combo(const char* label, const char* preview);
ResourceTree build_resource_tree(const std::vector<std::string>& paths);
std::optional<size_t> draw_resource_tree(
	const ResourceTree& tree,
	const std::vector<std::string>& paths,
	std::optional<size_t> selected = std::nullopt);
}
