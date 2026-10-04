#pragma once

#include <filesystem>
#include <string>
#include <memory>
#include <chrono>
#include <string_view>
#include <unordered_set>
#include <vector>

#include <quill/Logger.h>

#include "config.hpp"


// global singleton for convenience
class Utility
{
public:
	static RuntimePaths paths_for_executable(std::string_view app_name);
	static void initialize(RuntimePaths paths);

	// maintains consistent loop frequency, regardless of other compute within the loop
	struct LoopSleeper
	{
		LoopSleeper(std::chrono::milliseconds loop_period) : loop_period(loop_period) {}
		void operator()();

	private:
		const std::chrono::milliseconds loop_period;
		std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
	};

	// Engine defaults: resources/, shaders/, configs/, and precomputed lighting.
	static const std::filesystem::path& get_engine_runtime_path() { return get().paths.engine_runtime; }
	// Persistent app output; defaults to $XDG_DATA_HOME/krisp/<app_name>
	// (or ~/.local/share/krisp/<app_name>), unless RuntimePaths overrides it.
	static const std::filesystem::path& get_writable_data_path() { return get().paths.writable_data; }
	// Scene saves under the app's writable data directory.
	static std::filesystem::path get_saves_path() { return get_writable_data_path() / "saves"; }
	// Fixture directory supplied by set_test_mode; checked before app/engine assets.
	static const std::filesystem::path& get_test_data_path() { return get().test_data; }
	// Resolve a config path: existing app file first, then engine_runtime/configs.
	// The filename must be relative and contain no parent traversal.
	static std::filesystem::path get_config_path(std::string_view filename);
	// Destination for app config writes, without falling back to engine defaults.
	// The filename must be relative and contain no parent traversal.
	static std::filesystem::path get_user_config_path(std::string_view filename);

	static std::filesystem::path get_texture(std::string_view filename);
	static std::filesystem::path get_model(std::string_view filename);
	static std::filesystem::path get_animation(std::string_view filename);
	static std::filesystem::path get_shader(std::string_view filename);
	static std::filesystem::path get_audio(std::string_view filename);

	static std::vector<std::string> get_all_textures();
	static std::vector<std::string> get_all_models();
	static std::vector<std::string> get_all_animations();
	static std::vector<std::string> get_all_audio();
	static void set_test_mode(std::filesystem::path test_data);

	static quill::Logger* get_logger() { return get().logger; }

	// precision sleep uses a spin lock
	static void sleep(std::chrono::milliseconds duration, bool precise = false);

	static std::filesystem::path get_child(const std::filesystem::path& parent, const std::string_view child);

	static void enable_logging();

private:
	static Utility& get();
	explicit Utility(RuntimePaths paths);
	static std::filesystem::path get_rsrc_path(bool use_default = false);
	static std::filesystem::path resolve_resource(std::string_view subdir, std::string_view filename);
	static std::vector<std::string> collect_resources(
		std::string_view subdir,
		const std::unordered_set<std::string_view>& extensions);

	quill::Logger* logger;
	RuntimePaths paths;
	std::filesystem::path test_data;
	bool test_mode = false;
};
