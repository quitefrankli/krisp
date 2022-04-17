#pragma once

#include <filesystem>


namespace quill {
	class Logger;
}

// global singleton for convenience
class Utility
{
public:
	Utility();

	static Utility& get() { return singleton; }

	const std::filesystem::path& get_top_level_path() const { return top_level_dir; }
	const std::filesystem::path& get_shaders_path() const { return shaders; }
	const std::filesystem::path& get_model_path() const { return models; }
	const std::filesystem::path& get_textures_path() const { return textures; };
	const std::filesystem::path& get_build_path() const { return build; };
	const std::filesystem::path& get_binary_path() const { return binary; };
	const std::filesystem::path& get_audio_path() const { return audio; };

	quill::Logger* get_logger() { return logger; }

	static std::filesystem::path get_child(const std::filesystem::path& parent, const std::string_view child);

private:
	quill::Logger* logger;
	std::filesystem::path top_level_dir;
	std::filesystem::path shaders;
	std::filesystem::path models;
	std::filesystem::path textures;
	std::filesystem::path build;
	std::filesystem::path binary;
	std::filesystem::path audio;

	static Utility singleton;
};