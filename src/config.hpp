#pragma once

#include <string>
#include <string_view>
#include <memory>


class Config
{
public:
	static void initialise_global_config(std::string_view config_file_path);
	static bool enable_logging();
	static std::pair<int, int> get_window_pos();
	static bool is_raytracing_enabled();
};