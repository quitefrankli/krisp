#pragma once

#include <optional>
#include <string>
#include <string_view>

class ResourceLoadError;

namespace GuiWindowDetail
{
std::string report_resource_load_error(
	std::string_view context,
	const ResourceLoadError& error);
void draw_resource_load_error(const std::optional<std::string>& error);
bool begin_italic_combo(const char* label, const char* preview);
}
