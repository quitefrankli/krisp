#pragma once

#include <string>
#include <string_view>
#include <memory>


class Config
{
public:
	static void init(std::string_view project_name);
	static std::string_view get_project_name();
	static bool enable_logging();
	static std::pair<int, int> get_window_pos();
	static bool is_raytracing_enabled();
};