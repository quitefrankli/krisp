#pragma once

#include <string>
#include <chrono>
#include <functional>
#include <optional>


class Analytics
{
public:
	// period in seconds for logging to occur
	Analytics(const int period = 5);

	Analytics(std::function<void(float)>&& on_log, const int period = 5);

	//
	// Start and Stop
	// 	for measuring without spamming
	//	every LOG_PERIOD, the average is calculated and logged
	//	STOP must be preceeded by START
	//

	// begin timing
	void start();

	// stop timing
	void stop();

	void quick_timer_start();
	void quick_timer_stop();
	void quick_timer_stop(const std::string& mesg);
	std::string text;

private:
	uint64_t num_elapsed_cycles = 0;
	std::chrono::time_point<std::chrono::system_clock> log_cycle_start;
	std::chrono::time_point<std::chrono::system_clock> lap_cycle_start;
	std::chrono::nanoseconds elapsed_log_cycle = std::chrono::nanoseconds(0);
	std::chrono::time_point<std::chrono::system_clock> quick_timer_start_time;
	// once every X seconds Analytics::stop is called, data will be logged
	const std::chrono::seconds LOG_PERIOD;

	enum class State
	{
		FRESH,
		STARTED,
		STOPPED
	};

	State state = State::FRESH;

	// additional side effects on LOG_PERIOD
	std::optional<std::function<void(float)>> on_log_period;
};