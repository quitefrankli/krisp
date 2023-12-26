#pragma once

#include <filesystem>
#include <string>
#include <memory>
#include <chrono>
#include <optional>


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

	static Utility& get();

	const std::filesystem::path& get_top_level_path() const { return top_level_dir; }
	const std::filesystem::path& get_build_path() const { return build; };
	const std::filesystem::path& get_shaders_path() const { return shaders; }
	const std::filesystem::path& get_binary_path() const { return binary; };
	const std::filesystem::path& get_config_path() const { return config; };
	const std::filesystem::path& get_model_path() const { return models; }
	const std::filesystem::path& get_textures_path() const { return textures; }
	const std::filesystem::path& get_audio_path() const { return audio; }

	void set_appname_for_path(const std::string_view app);
	std::filesystem::path get_app_model_path() const;
	std::filesystem::path get_app_textures_path() const;
	std::filesystem::path get_app_audio_path() const;

	static std::string get_texture(const std::string_view texture);
	static std::string get_model(const std::string_view model);
	static float get_rand(float min, float max);

	quill::Logger* get_logger() { return logger; }

	// precision sleep uses a spin lock
	static void sleep(std::chrono::milliseconds duration, bool precise = false);

	static std::filesystem::path get_child(const std::filesystem::path& parent, const std::string_view child);

	void enable_logging();

private:
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