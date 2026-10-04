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
#include <cstdlib>
#include <iostream>
#include <chrono>
#include <thread>
#include <random>
#include <algorithm>
#include <system_error>


namespace
{
	std::unique_ptr<Utility> utility_singleton;

	std::filesystem::path xdg_directory(const char* variable, const char* home_suffix, std::string_view app_name)
	{
		if (const char* configured = std::getenv(variable); configured && *configured)
			return std::filesystem::path(configured) / app_name;
		if (const char* home = std::getenv("HOME"); home && *home)
			return std::filesystem::path(home) / home_suffix / app_name;
		return std::filesystem::temp_directory_path() / app_name;
	}

	void validate_relative_filename(std::string_view filename, std::string_view operation)
	{
		const std::filesystem::path relative(filename);
		if (relative.empty() || relative.is_absolute()
			|| std::ranges::find(relative, std::filesystem::path("..")) != relative.end())
		{
			throw std::runtime_error(fmt::format(
				"{}: expected a resource-relative filename: '{}'", operation, filename));
		}
	}
}

RuntimePaths Utility::paths_for_executable(std::string_view app_name)
{
	std::error_code error;
	auto executable = std::filesystem::read_symlink("/proc/self/exe", error);
	if (error)
		throw std::runtime_error(fmt::format("Utility::paths_for_executable: cannot resolve executable: {}", error.message()));
	const auto executable_dir = executable.parent_path();
	return RuntimePaths{
		.app_resources = executable_dir / "resources" / app_name,
		.app_config = xdg_directory("XDG_CONFIG_HOME", ".config", app_name),
		.engine_runtime = executable_dir / "krisp-runtime",
		.writable_data = xdg_directory("XDG_DATA_HOME", ".local/share", fmt::format("krisp/{}", app_name)),
	};
}

void Utility::initialize(RuntimePaths paths)
{
	if (utility_singleton)
		throw std::runtime_error("Utility::initialize: runtime paths are already configured");
	if (paths.app_resources.empty() || paths.app_config.empty()
		|| paths.engine_runtime.empty() || paths.writable_data.empty())
		throw std::invalid_argument("Utility::initialize: all runtime paths must be provided");
	utility_singleton.reset(new Utility(std::move(paths)));
}

Utility::Utility(RuntimePaths runtime_paths) : paths(std::move(runtime_paths))
{
	const auto log_dir = paths.writable_data / "logs";
	std::filesystem::create_directories(log_dir);

	quill::FileSinkConfig file_sink_config{};
	file_sink_config.set_open_mode('a');
	auto file_sink = quill::Frontend::create_or_get_sink<quill::FileSink>(
		(log_dir / "krisp.log").string(), file_sink_config);

	quill::ConsoleSinkConfig console_sink_config;
	console_sink_config.set_stream("stderr");
	auto console_sink = quill::Frontend::create_or_get_sink<quill::ConsoleSink>(
		"stderr",
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
			 "Utility::Utility: Initialised with runtime:{}, data:{}",
			 get_engine_runtime_path().string(),
			 get_writable_data_path().string());
}

Utility& Utility::get()
{
	if (!utility_singleton)
		throw std::runtime_error("Utility used before Config::init configured runtime paths");
	return *utility_singleton;
}

std::filesystem::path Utility::resolve_resource(std::string_view subdir, std::string_view filename)
{
	validate_relative_filename(filename, "Utility::resolve_resource");
	const std::filesystem::path resource_name(filename);

	if (get().test_mode)
	{
		auto test_path = get().test_data / resource_name;
		if (std::filesystem::exists(test_path))
			return test_path;
	}

	auto app_path = get_rsrc_path() / subdir / resource_name;
	if (std::filesystem::exists(app_path))
	{
		return app_path;
	}

	auto default_path = get_rsrc_path(true) / subdir / resource_name;
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
	validate_relative_filename(filename, "Utility::get_config_path");
	auto app_config = get().paths.app_config / filename;
	if (std::filesystem::exists(app_config))
	{
		return app_config;
	}
	return get().paths.engine_runtime / "configs" / filename;
}

std::filesystem::path Utility::get_user_config_path(std::string_view filename)
{
	validate_relative_filename(filename, "Utility::get_user_config_path");
	return get().paths.app_config / filename;
}

std::filesystem::path Utility::get_rsrc_path(bool use_default)
{
	if (use_default)
	{
		return get().paths.engine_runtime / "resources";
	}

	return get().paths.app_resources;
}

std::filesystem::path Utility::get_texture(std::string_view filename)
{
	return resolve_resource("textures", filename);
}

std::filesystem::path Utility::get_model(std::string_view filename)
{
	return resolve_resource("meshes", filename);
}

std::filesystem::path Utility::get_animation(std::string_view filename)
{
	return resolve_resource("animations", filename);
}

std::filesystem::path Utility::get_shader(std::string_view filename)
{
	validate_relative_filename(filename, "Utility::get_shader");
	auto app_path = get_rsrc_path() / "shaders" / filename;
	if (std::filesystem::exists(app_path))
		return app_path;
	return get().paths.engine_runtime / "shaders" / filename;
}

void Utility::set_test_mode(std::filesystem::path test_data)
{
	if (test_data.empty() || !test_data.is_absolute())
		throw std::invalid_argument("Utility::set_test_mode: test data path must be absolute");
	get().test_data = std::move(test_data);
	get().test_mode = true;
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

std::vector<std::string> Utility::collect_resources(
	std::string_view subdir,
	const std::unordered_set<std::string_view>& extensions)
{
	std::unordered_set<std::string> seen_filenames;
	std::vector<std::string> files;

	auto collect_from = [&](const std::filesystem::path& dir) {
		if (!std::filesystem::exists(dir)) return;
		for (const auto& entry : std::filesystem::recursive_directory_iterator(dir))
		{
			if (entry.is_regular_file() && extensions.contains(entry.path().extension().string()))
			{
				auto filename = entry.path().lexically_relative(dir).generic_string();
				if (seen_filenames.insert(filename).second)
					files.push_back(std::move(filename));
			}
		}
	};

	collect_from(get_rsrc_path() / subdir);
	collect_from(get_rsrc_path(true) / subdir);

	return files;
}

std::vector<std::string> Utility::get_all_textures()
{
	return collect_resources("textures", { ".jpg", ".jpeg", ".png", ".bmp", ".tga", ".dds" });
}

std::vector<std::string> Utility::get_all_models()
{
	return collect_resources("meshes", { ".gltf", ".glb", ".obj", ".fbx" });
}

std::vector<std::string> Utility::get_all_animations()
{
	return collect_resources("animations", { ".gltf", ".glb" });
}

std::vector<std::string> Utility::get_all_audio()
{
	return collect_resources("sound", { ".wav", ".ogg", ".mp3", ".flac" });
}

void Utility::LoopSleeper::operator()()
{
	const auto margin_of_error = std::chrono::milliseconds(1); // system can only sleep longer than requested

	while (true)
	{
		const auto elapsed = std::chrono::steady_clock::now() - start;
		if (elapsed > (loop_period - margin_of_error))
		{
			break;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	start = std::chrono::steady_clock::now();
}
