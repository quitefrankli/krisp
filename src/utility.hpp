#pragma once

#include <filesystem>
#include <string>
#include <memory>
#include <chrono>
#include <optional>
#include <unordered_set>


namespace quill {
	class Logger;
}

// global singleton for convenience
class Utility
{
public:
	Utility();

	// maintains consistent loop frequency, regardless of other compute within the loop
	struct LoopSleeper
	{
		LoopSleeper(std::chrono::milliseconds loop_period);
		~LoopSleeper();
		void operator()();

	private:
		const std::chrono::milliseconds loop_period;
		std::chrono::system_clock::time_point start = std::chrono::system_clock::now();
		struct Pimpl;
		std::unique_ptr<Pimpl> pimpl;
	};

	static const std::filesystem::path& get_top_level_path() { return get().top_level_dir; }
	static const std::filesystem::path& get_build_path() { return get().build; };
	static const std::filesystem::path& get_shaders_path() { return get().shaders; }
	static const std::filesystem::path& get_binary_path() { return get().binary; };
	static const std::filesystem::path& get_config_path() { return get().config; };
	static const std::filesystem::path& get_model_path() { return get().models; }
	static const std::filesystem::path& get_textures_path() { return get().textures; }
	static const std::filesystem::path& get_audio_path() { return get().audio; }

	static void set_appname_for_path(const std::string_view app);
	static std::filesystem::path get_app_model_path();
	static std::filesystem::path get_app_textures_path();
	static std::filesystem::path get_app_audio_path();

	static std::string get_texture(const std::string_view texture);
	static std::string get_model(const std::string_view model);
	static float get_rand(float min, float max);
	static std::vector<std::filesystem::path> get_all_files(const std::filesystem::path& path, 
												  			const std::unordered_set<std::string_view>& extensions);

	static quill::Logger* get_logger() { return get().logger; }

	// precision sleep uses a spin lock
	static void sleep(std::chrono::milliseconds duration, bool precise = false);

	static std::filesystem::path get_child(const std::filesystem::path& parent, const std::string_view child);

	static void enable_logging();

private:
	static Utility& get();

	quill::Logger* logger;
	std::filesystem::path top_level_dir;
	std::filesystem::path shaders;
	std::filesystem::path models;
	std::filesystem::path textures;
	std::filesystem::path build;
	std::filesystem::path binary;
	std::filesystem::path audio;
	std::filesystem::path config;
	std::optional<std::filesystem::path> app_rsrc_path;
	
	struct UtilityImpl;
	static std::unique_ptr<UtilityImpl> impl;
};