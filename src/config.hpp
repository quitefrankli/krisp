#pragma once

#include <filesystem>
#include <string_view>
#include <utility>

struct RuntimePaths
{
	std::filesystem::path app_resources;
	std::filesystem::path app_config;
	std::filesystem::path engine_runtime;
	std::filesystem::path writable_data;
};

class Config
{
public:
	// Initialize with default executable-relative assets and XDG user directories.
	static void init(std::string_view app_name);
	// Initialize with explicit paths for checkout development or custom layouts.
	static void init(std::string_view app_name, RuntimePaths paths);
	static std::string_view get_project_name();
	static bool enable_logging();
	static std::pair<int, int> get_window_pos();
	static bool is_raytracing_enabled();
};
