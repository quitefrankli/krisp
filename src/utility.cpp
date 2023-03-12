#include "utility.hpp"

#include <quill/Quill.h>

#include <iostream>
#include <chrono>
#include <thread>
#include <random>


Utility Utility::singleton;

Utility::Utility()
{
	top_level_dir = PROJECT_TOP_LEVEL_SRC_DIR;
	models = get_child(top_level_dir, "resources/models");
	textures = get_child(top_level_dir, "resources/textures");
	audio = get_child(top_level_dir, "resources/sound");
	
	build = PROJECT_BUILD_DIR;
	binary = PROJECT_BIN_DIR;
	shaders = get_child(build, "shaders");

	auto file_handler = quill::file_handler("log.log", "a");
	logger = quill::create_logger("MAIN", file_handler);
	file_handler->set_pattern(
		QUILL_STRING("%(ascii_time): %(message)"),
		"%D %H:%M:%S.%Qus",
		quill::Timezone::LocalTime
	);

#ifdef ENABLE_LOGGING
	quill::start(); // this will consume CPU cycles
#endif

	LOG_INFO(logger, "Utility::Utility: Initialised with models:{}, textures:{}, build:{}, binary{}", models, textures, build, binary);
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
	const auto start = std::chrono::high_resolution_clock::now();
	// this was discovered through trial and error, it would appear that a 1ms sleep takes ~ 15ms on windows
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

void Utility::LoopSleeper::operator()()
{
	const auto now = std::chrono::system_clock::now();
	const auto elapsed = now - start;

	if (elapsed < loop_period)
	{
		std::this_thread::sleep_for(loop_period - elapsed);
		// Utility::sleep(std::chrono::duration_cast<std::chrono::milliseconds>(loop_period - elapsed));
	}

	start = std::chrono::system_clock::now();
}