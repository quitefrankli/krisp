#include "utility.hpp"

#include <quill/Quill.h>

#include <iostream>
#include <chrono>
#include <thread>
#include <random>


Utility::Utility()
{
	top_level_dir = PROJECT_TOP_LEVEL_SRC_DIR;
	models = get_child(top_level_dir, "resources/models");
	textures = get_child(top_level_dir, "resources/textures");
	audio = get_child(top_level_dir, "resources/sound");
	config = get_child(top_level_dir, "configs");
	
	build = PROJECT_BUILD_DIR;
	binary = PROJECT_BIN_DIR;
	shaders = get_child(build, "shaders");

	auto file_handler = quill::file_handler(fmt::format("{}/log.log", PROJECT_TOP_LEVEL_SRC_DIR), "a");
	logger = quill::create_logger("MAIN", file_handler);

	// guarantees a blocking flush when a log level at or higher than this is logged
	logger->init_backtrace(10, quill::LogLevel::Error);

	file_handler->set_pattern(
		QUILL_STRING("%(ascii_time): %(message)"),
		"%D %H:%M:%S.%Qus",
		quill::Timezone::LocalTime
	);
}

void Utility::enable_logging()
{
	quill::start(); // this will consume CPU cycles
	LOG_INFO(logger, "Utility::Utility: Initialised with models:{}, textures:{}, build:{}, binary{}", models, textures, build, binary);
}

Utility& Utility::get()
{
	// inspired by meyer's singleton
	static Utility singleton;
	return singleton;
}

void Utility::set_appname_for_path(const std::string_view app)
{
	app_rsrc_path = get_top_level_path() / "resources/applications" / app.data();
}

std::filesystem::path Utility::get_app_model_path() const
{
	return app_rsrc_path ? app_rsrc_path.value() / "models" : models;
}

std::filesystem::path Utility::get_app_textures_path() const
{
	return app_rsrc_path ? app_rsrc_path.value() / "textures" : textures;
}

std::filesystem::path Utility::get_app_audio_path() const
{
	return app_rsrc_path ? app_rsrc_path.value() / "sound" : audio;
}

std::string Utility::get_texture(const std::string_view texture)
{
	return get().get_textures_path().string() + '/' + texture.data();
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

std::unique_ptr<Utility::UtilityImpl> Utility::impl = std::make_unique<Utility::UtilityImpl>();


#ifdef _WIN32
#include <windows.h>    /* WinAPI */

struct Utility::LoopSleeper::Pimpl
{
public:
	Pimpl(int milliseconds)
	{
		// Request high resolution timer
		static MMRESULT result = timeBeginPeriod(timer_period);
		assert(result == TIMERR_NOERROR);

		/* Create timer */
		timer = CreateWaitableTimer(NULL, FALSE, NULL);
		assert(timer);

		/* Set timer properties */
		li.QuadPart = -milliseconds;
		auto res = SetWaitableTimer(timer, &li, milliseconds, NULL, NULL, FALSE);
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

/* Unix sleep in 100ns units */
int nanosleep(const struct timespec *req, struct timespec *rem){
	struct timespec temp_rem;
	if(nanosleep(req, rem) == -1)
		nanosleep(rem, &temp_rem);
	else
		return 1;
}
#endif

Utility::LoopSleeper::LoopSleeper(std::chrono::milliseconds loop_period) :
	loop_period(loop_period),
	pimpl(std::make_unique<Pimpl>(loop_period.count()))
{
}

Utility::LoopSleeper::~LoopSleeper()
{
}

void Utility::LoopSleeper::operator()()
{
#ifdef _WIN32
	pimpl->sleep();
#elif defined(__unix__) || defined(__APPLE__)
	// nanosleep(loop_period.count() * 1e6);
#endif

	// const auto now = std::chrono::system_clock::now();
	// const auto margin_of_error = std::chrono::milliseconds(1); // system can only sleep longer than requested
	// auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start);
	// std::chrono::nanoseconds sleep_ns = loop_period - elapsed;
	// // nanosleep(sleep_ns.count()/1000);
	// // nanosleep(3e6);
	// // Sleep(10);
	// // std::this_thread::sleep_for(std::chrono::milliseconds(17));
	// // nanosleep(17e6);

	// // while (elapsed < loop_period - margin_of_error)
	// // {
	// // 	std::this_thread::sleep_for(std::chrono::milliseconds(1));
	// // 	elapsed = std::chrono::system_clock::now() - start;
	// // }

	// start = std::chrono::system_clock::now();
}