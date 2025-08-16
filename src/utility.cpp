#include "utility.hpp"

#include <quill/LogMacros.h>
#include <quill/Backend.h>
#include <quill/Frontend.h>
#include <quill/sinks/FileSink.h>
#include <quill/sinks/ConsoleSink.h>
#include <quill/filters/Filter.h>
#include <fmt/core.h>

#include <iostream>
#include <chrono>
#include <thread>
#include <random>


Utility::Utility()
{
	top_level_dir = PROJECT_TOP_LEVEL_SRC_DIR;
	config = get_child(top_level_dir, "configs");
	
	models = get_child(top_level_dir, "resources/models");
	textures = get_child(top_level_dir, "resources/textures");
	audio = get_child(top_level_dir, "resources/sound");
	
	build = PROJECT_BUILD_DIR;
	binary = PROJECT_BIN_DIR;
	shaders = get_child(build, "shaders");

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

	// create all the directories if they do not exist
	std::filesystem::create_directories(models);
	std::filesystem::create_directories(textures);
	std::filesystem::create_directories(audio);
}

void Utility::enable_logging()
{
	quill::BackendOptions backend_options;
	quill::Backend::start(backend_options); // this will consume CPU cycles
	LOG_INFO(get_logger(), 
			 "Utility::Utility: Initialised with models:{}, textures:{}, build:{}, binary{}", 
			 get_model_path().string(), 
			 get_textures_path().string(), 
			 get_build_path().string(), 
			 get_binary_path().string());
}

Utility& Utility::get()
{
	// inspired by meyer's singleton
	static Utility singleton;
	return singleton;
}

std::string Utility::get_texture(const std::string_view texture)
{
	return get().get_textures_path().string() + '/' + texture.data();
}

std::string Utility::get_model(const std::string_view model)
{
	return get().get_model_path().string() + '/' + model.data();
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

struct Utility::UtilityImpl
{
	UtilityImpl() : rng(rd())
	{
	}

	std::random_device rd;
	std::mt19937_64 rng;
};

float Utility::get_rand(float min, float max)
{
	return std::uniform_real_distribution(min, max)(impl->rng);
}

std::vector<std::filesystem::path> Utility::get_all_files(const std::filesystem::path& path,
                                                		  const std::unordered_set<std::string_view>& extensions)
{
	std::vector<std::filesystem::path> files;
	for (const auto& entry : std::filesystem::directory_iterator(path))
	{
		if (entry.is_regular_file() && extensions.contains(entry.path().extension().string()))
		{
			files.push_back(entry);
		}
	}

	return files;
}

std::unique_ptr<Utility::UtilityImpl> Utility::impl = std::make_unique<Utility::UtilityImpl>();


#ifdef _WIN32
#include <windows.h>    /* WinAPI */
#include <mmsystem.h>   /* timeBeginPeriod, timeEndPeriod */

struct Utility::LoopSleeper::Pimpl
{
public:
	Pimpl(LoopSleeper& loop_sleeper)
	{
		// Request high resolution timer
		static MMRESULT result = timeBeginPeriod(timer_period);
		assert(result == TIMERR_NOERROR);

		/* Create timer */
		timer = CreateWaitableTimer(NULL, FALSE, NULL);
		assert(timer);

		/* Set timer properties */
		li.QuadPart = -loop_sleeper.loop_period.count();
		auto res = SetWaitableTimer(timer, &li, loop_sleeper.loop_period.count(), NULL, NULL, FALSE);
		assert(res);
	}

	~Pimpl()
	{
		/* Clean resources */
		CloseHandle(timer);

		// Release high resolution timer
		static MMRESULT result = timeEndPeriod(timer_period);
		assert(result == TIMERR_NOERROR);
	}

	void sleep()
	{
		WaitForSingleObject(timer, INFINITE);
	}

    HANDLE timer;   /* Timer handle */
    LARGE_INTEGER li{};   /* Time defintion */
	const int timer_period = 1;
};

#elif defined(__unix__) || defined(__APPLE__)
#include <time.h>   /* nanosleep */

// /* Unix sleep in 100ns units */
// int nanosleep(const struct timespec *req, struct timespec *rem){
// 	struct timespec temp_rem;
// 	if(nanosleep(req, rem) == -1)
// 		nanosleep(rem, &temp_rem);
// 	else
// 		return 1;
// }

struct Utility::LoopSleeper::Pimpl
{
public:
	Pimpl(LoopSleeper& loop_sleeper) : loop_sleeper(loop_sleeper)
	{
	}

	void sleep()
	{
		const auto start = std::chrono::system_clock::now();
		const auto margin_of_error = std::chrono::milliseconds(1); // system can only sleep longer than requested

		while (true)
		{
			const auto elapsed = std::chrono::system_clock::now() - start;
			if (elapsed > (loop_sleeper.loop_period - margin_of_error))
			{
				break;
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
}

	LoopSleeper& loop_sleeper;
};

#endif

Utility::LoopSleeper::LoopSleeper(std::chrono::milliseconds loop_period) :
	loop_period(loop_period),
	pimpl(std::make_unique<Pimpl>(*this))
{
}

Utility::LoopSleeper::~LoopSleeper()
{
}

void Utility::LoopSleeper::operator()()
{
	pimpl->sleep();
}