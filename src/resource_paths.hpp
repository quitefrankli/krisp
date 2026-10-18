#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

struct EnginePaths
{
	// Read-only locations; source assets and generated data can live separately.
	std::filesystem::path resources;
	std::filesystem::path shaders;
	std::filesystem::path default_config;
	std::filesystem::path environment_lighting;
};

struct RuntimePaths
{
	std::filesystem::path app_resources;
	std::filesystem::path app_config;
	std::filesystem::path writable_data;
	EnginePaths engine;
};

// Central access point for engine and application resources and paths.
class ResourcePaths
{
public:
	// Configure once before resource lookup (normally through Config::init).
	static void initialize(RuntimePaths paths);
	// Engine defaults come from this library's build; app assets are executable-relative.
	static RuntimePaths paths_for_executable(std::string_view app_name);

	static const EnginePaths& get_engine_paths();
	static const std::filesystem::path& get_writable_data_path();
	static const std::filesystem::path& get_test_data_path();
	// Existing app configuration overrides the engine default; writes stay app-local.
	static std::filesystem::path get_config_path(std::string_view filename);
	static std::filesystem::path get_user_config_path(std::string_view filename);

	// Relative names cannot contain '..'. Lookup prefers test fixtures (when enabled),
	// then app resources, then engine defaults. Missing resources throw.
	static std::filesystem::path get_texture(std::string_view filename);
	static std::filesystem::path get_resource(std::string_view filename);
	static std::filesystem::path get_model(std::string_view filename);
	static std::filesystem::path get_animation(std::string_view filename);
	static std::filesystem::path get_shader(std::string_view filename);
	static std::filesystem::path get_audio(std::string_view filename);

	static std::vector<std::string> get_all_textures();
	static std::vector<std::string> get_all_models();
	static std::vector<std::string> get_all_animations();
	static std::vector<std::string> get_all_audio();
	static void set_test_mode(std::filesystem::path test_data);

private:
	static ResourcePaths& get();
	explicit ResourcePaths(RuntimePaths paths);
	static std::filesystem::path get_rsrc_path(bool use_default = false);
	static std::filesystem::path resolve_resource(std::string_view subdir, std::string_view filename);
	static std::vector<std::string> collect_resources(
		std::string_view subdir,
		const std::unordered_set<std::string_view>& extensions);

	RuntimePaths paths;
	std::filesystem::path test_data;
	bool test_mode = false;
};
