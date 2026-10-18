#pragma once

#include <filesystem>
#include <chrono>
#include <string_view>

#include <quill/Logger.h>

class Utility
{
public:
	static void initialize(std::filesystem::path writable_data);

	struct LoopSleeper
	{
		LoopSleeper(std::chrono::milliseconds loop_period) : loop_period(loop_period) {}
		void operator()();

	private:
		const std::chrono::milliseconds loop_period;
		std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
	};

	static const std::filesystem::path& get_writable_data_path();
	static std::filesystem::path get_saves_path();
	static quill::Logger* get_logger() { return get().logger; }
	static void sleep(std::chrono::milliseconds duration, bool precise = false);
	static std::filesystem::path get_child(const std::filesystem::path& parent, std::string_view child);
	static void enable_logging();

private:
	static Utility& get();
	explicit Utility(std::filesystem::path writable_data);

	quill::Logger* logger;
	std::filesystem::path writable_data;
};
