#pragma once

#include <filesystem>
#include <string>
#include <memory>
#include <chrono>
#include <unordered_set>

#include <quill/Logger.h>


// global singleton for convenience
class Utility
{
public:
	Utility();

	// maintains consistent loop frequency, regardless of other compute within the loop
	struct LoopSleeper
	{
		LoopSleeper(std::chrono::milliseconds loop_period) : loop_period(loop_period) {}
		void operator()();

	private:
		const std::chrono::milliseconds loop_period;
		std::chrono::system_clock::time_point start = std::chrono::system_clock::now();
	};

	static const std::filesystem::path& get_top_level_path() { return get().top_level_dir; }
	static const std::filesystem::path& get_build_path() { return get().build; };
	static const std::filesystem::path& get_binary_path() { return get().binary; };
	static std::filesystem::path get_config_path();

	static std::filesystem::path get_texture(std::string_view filename);
	static std::filesystem::path get_model(std::string_view filename);
	static std::filesystem::path get_shader(std::string_view filename);
	static std::filesystem::path get_audio(std::string_view filename);

	static std::vector<std::filesystem::path> get_all_textures();
	static std::vector<std::filesystem::path> get_all_models();
	static std::vector<std::filesystem::path> get_all_audio();

	static quill::Logger* get_logger() { return get().logger; }

	// precision sleep uses a spin lock
	static void sleep(std::chrono::milliseconds duration, bool precise = false);

	static std::filesystem::path get_child(const std::filesystem::path& parent, const std::string_view child);

	static void enable_logging();

private:
	static Utility& get();
	static std::filesystem::path get_rsrc_path(bool use_default = false);
	static std::filesystem::path resolve_resource(std::string_view subdir, std::string_view filename);
	static std::vector<std::filesystem::path> collect_resources(std::string_view subdir, const std::unordered_set<std::string_view>& extensions);

	quill::Logger* logger;
	std::filesystem::path top_level_dir;
	std::filesystem::path build;
	std::filesystem::path binary;
};
