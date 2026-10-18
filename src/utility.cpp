#include "utility.hpp"
#include "resource_paths.hpp"

#include <quill/LogMacros.h>
#include <quill/Backend.h>
#include <quill/Frontend.h>
#include <quill/sinks/FileSink.h>
#include <quill/sinks/ConsoleSink.h>
#include <fmt/core.h>

#include <cmath>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace
{
	std::unique_ptr<Utility> utility_singleton;
}

void Utility::initialize(std::filesystem::path writable_data)
{
	if (utility_singleton)
		throw std::runtime_error("Utility::initialize: logging is already configured");
	if (writable_data.empty())
		throw std::invalid_argument("Utility::initialize: writable data path must be provided");
	utility_singleton.reset(new Utility(std::move(writable_data)));
}

Utility::Utility(std::filesystem::path data_path) : writable_data(std::move(data_path))
{
	const auto log_dir = writable_data / "logs";
	std::filesystem::create_directories(log_dir);

	quill::FileSinkConfig file_sink_config{};
	file_sink_config.set_open_mode('a');
	auto file_sink = quill::Frontend::create_or_get_sink<quill::FileSink>(
		(log_dir / "krisp.log").string(), file_sink_config);

	quill::ConsoleSinkConfig console_sink_config;
	console_sink_config.set_stream("stderr");
	auto console_sink = quill::Frontend::create_or_get_sink<quill::ConsoleSink>("stderr", console_sink_config);
	console_sink->set_log_level_filter(quill::LogLevel::Error);

	quill::PatternFormatterOptions pattern_formatter_options;
	pattern_formatter_options.format_pattern = "[%(log_level)] %(time): %(message)";
	pattern_formatter_options.timestamp_pattern = "%D %H:%M:%S.%Qns";
	pattern_formatter_options.timestamp_timezone = quill::Timezone::LocalTime;

	std::vector<std::shared_ptr<quill::Sink>> sinks = { std::move(file_sink), std::move(console_sink) };
	logger = quill::Frontend::create_or_get_logger("MAIN", sinks, pattern_formatter_options);
	logger->init_backtrace(10, quill::LogLevel::Error);
}

void Utility::enable_logging()
{
	quill::BackendOptions backend_options;
	quill::Backend::start(backend_options);
	const auto& engine = ResourcePaths::get_engine_paths();
	LOG_INFO(get_logger(), "Utility::Utility: Initialised with runtime:{}, data:{}",
		engine.resources.string(), get_writable_data_path().string());
}

Utility& Utility::get()
{
	if (!utility_singleton)
		throw std::runtime_error("Utility used before Config::init configured logging");
	return *utility_singleton;
}

const std::filesystem::path& Utility::get_writable_data_path()
{
	return ResourcePaths::get_writable_data_path();
}

std::filesystem::path Utility::get_saves_path()
{
	return get_writable_data_path() / "saves";
}

std::filesystem::path Utility::get_child(const std::filesystem::path& parent, std::string_view child)
{
	return parent / std::filesystem::path(std::string(child));
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
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		while (std::chrono::high_resolution_clock::now() < precise_end) {}
	}
	else
		std::this_thread::sleep_for(duration);
}

void Utility::LoopSleeper::operator()()
{
	const auto margin_of_error = std::chrono::milliseconds(1);
	while (true)
	{
		const auto elapsed = std::chrono::steady_clock::now() - start;
		if (elapsed > (loop_period - margin_of_error)) break;
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	start = std::chrono::steady_clock::now();
}
