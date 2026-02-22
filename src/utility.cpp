#include "utility.hpp"
#include "config.hpp"

#include <quill/LogMacros.h>
#include <quill/Backend.h>
#include <quill/Frontend.h>
#include <quill/sinks/FileSink.h>
#include <quill/sinks/ConsoleSink.h>
#include <quill/filters/Filter.h>
#include <fmt/core.h>

#include <ctime>
#include <iostream>
#include <chrono>
#include <thread>
#include <random>


Utility::Utility()
{
	top_level_dir = PROJECT_TOP_LEVEL_SRC_DIR;
	build = PROJECT_BUILD_DIR;
	binary = PROJECT_BIN_DIR;

	quill::FileSinkConfig file_sink_config{};
	file_sink_config.set_open_mode('a');
	auto file_sink = quill::Frontend::create_or_get_sink<quill::FileSink>(
		fmt::format("{}/log.log", PROJECT_TOP_LEVEL_SRC_DIR), file_sink_config);

	quill::ConsoleSinkConfig console_sink_config;
	auto console_sink = quill::Frontend::create_or_get_sink<quill::ConsoleSink>(
		"stdout",
		console_sink_config);
	console_sink->set_log_level_filter(quill::LogLevel::Error);

	quill::PatternFormatterOptions pattern_formatter_options;
	pattern_formatter_options.format_pattern = "[%(log_level)] %(time): %(message)";
	pattern_formatter_options.timestamp_pattern = "%D %H:%M:%S.%Qns";
	pattern_formatter_options.timestamp_timezone = quill::Timezone::LocalTime;
	
	std::vector<std::shared_ptr<quill::Sink>> sinks = { std::move(file_sink), std::move(console_sink) };

	logger = quill::Frontend::create_or_get_logger(
		"MAIN", 
		sinks,
		pattern_formatter_options);

	// guarantees a blocking flush when a log level at or higher than this is logged
	logger->init_backtrace(10, quill::LogLevel::Error);
}

void Utility::enable_logging()
{
	quill::BackendOptions backend_options;
	quill::Backend::start(backend_options); // this will consume CPU cycles
	LOG_INFO(get_logger(),
			 "Utility::Utility: Initialised with build:{}, binary:{}",
			 get_build_path().string(),
			 get_binary_path().string());
}

Utility& Utility::get()
{
	// inspired by meyer's singleton
	static Utility singleton;
	return singleton;
}

std::filesystem::path Utility::resolve_resource(std::string_view subdir, std::string_view filename)
{
	auto app_path = get_rsrc_path() / subdir / filename;
	if (std::filesystem::exists(app_path))
	{
		return app_path;
	}

	auto default_path = get_rsrc_path(true) / subdir / filename;
	if (std::filesystem::exists(default_path))
	{
		return default_path;
	}

	throw std::runtime_error(fmt::format(
		"Utility::resolve_resource: resource not found in app/default paths. subdir='{}', filename='{}'",
		subdir,
		filename));
}

std::filesystem::path Utility::get_config_path(std::string_view filename)
{
	auto app_config = get().top_level_dir / "applications" / Config::get_project_name() / filename;
	if (std::filesystem::exists(app_config))
	{
		return app_config;
	}
	return get().top_level_dir / "configs" / filename;
}

std::filesystem::path Utility::get_rsrc_path(bool use_default)
{
	if (use_default)
	{
		return get().top_level_dir / "resources/applications/default";
	}

	return get().top_level_dir / "resources/applications" / Config::get_project_name();
}

std::filesystem::path Utility::get_texture(std::string_view filename)
{
	return resolve_resource("textures", filename);
}

std::filesystem::path Utility::get_model(std::string_view filename)
{
	return resolve_resource("models", filename);
}

std::filesystem::path Utility::get_shader(std::string_view filename)
{
	auto app_path = get_rsrc_path() / "shaders" / filename;
	if (std::filesystem::exists(app_path))
		return app_path;
	return get().build / "shaders" / filename;
}

std::filesystem::path Utility::get_audio(std::string_view filename)
{
	return resolve_resource("sound", filename);
}

std::filesystem::path Utility::get_child(const std::filesystem::path& parent, const std::string_view child)
{
	return std::filesystem::path(parent.string() + '/' + child.data());
}

void Utility::sleep(std::chrono::milliseconds duration, bool precise)
{
	if (precise)
	{
		const auto start = std::chrono::high_resolution_clock::now();
		const float precision_ms = 20.0f;
		const auto imprecise_end = start + (duration - std::chrono::milliseconds(int(std::round(precision_ms))));
		const auto precise_end = start + duration;
		while (std::chrono::high_resolution_clock::now() < imprecise_end)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}

		// spin-lock
		while (std::chrono::high_resolution_clock::now() < precise_end)
		{
		}
	} else
	{
		std::this_thread::sleep_for(duration);
	}
}

std::vector<std::filesystem::path> Utility::collect_resources(std::string_view subdir,
                                                              const std::unordered_set<std::string_view>& extensions)
{
	std::unordered_set<std::string> seen_filenames;
	std::vector<std::filesystem::path> files;

	auto collect_from = [&](const std::filesystem::path& dir) {
		if (!std::filesystem::exists(dir)) return;
		for (const auto& entry : std::filesystem::directory_iterator(dir))
		{
			if (entry.is_regular_file() && extensions.contains(entry.path().extension().string()))
			{
				auto filename = entry.path().filename().string();
				if (!seen_filenames.contains(filename))
				{
					seen_filenames.insert(filename);
					files.push_back(entry);
				}
			}
		}
	};

	collect_from(get_rsrc_path() / subdir);
	collect_from(get_rsrc_path(true) / subdir);

	return files;
}

std::vector<std::filesystem::path> Utility::get_all_textures()
{
	return collect_resources("textures", { ".jpg", ".jpeg", ".png", ".bmp", ".tga" });
}

std::vector<std::filesystem::path> Utility::get_all_models()
{
	return collect_resources("models", { ".gltf", ".glb", ".obj", ".fbx" });
}

std::vector<std::filesystem::path> Utility::get_all_audio()
{
	return collect_resources("sound", { ".wav", ".ogg", ".mp3", ".flac" });
}

void Utility::LoopSleeper::operator()()
{
	const auto start = std::chrono::system_clock::now();
	const auto margin_of_error = std::chrono::milliseconds(1); // system can only sleep longer than requested

	while (true)
	{
		const auto elapsed = std::chrono::system_clock::now() - start;
		if (elapsed > (loop_period - margin_of_error))
		{
			break;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
}
