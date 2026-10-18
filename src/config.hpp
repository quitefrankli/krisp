#pragma once

#include <string_view>
#include <utility>

#include "resource_paths.hpp"

class Config
{
public:
	static void init(std::string_view app_name);
	static void init(std::string_view app_name, RuntimePaths paths);
	static std::string_view get_project_name();
	static bool enable_logging();
	static std::pair<int, int> get_window_pos();
	static bool is_raytracing_enabled();
};
