#pragma once

#include "utility_functions.hpp"

#include <string>
#include <chrono>


class Analytics
{
public:
	void start();
	void stop();
	void quick_timer_start();
	void quick_timer_stop();
	std::string text;

private:
	bool has_started = false;
	uint64_t num_elapsed_cycles = 0;
	std::chrono::time_point<std::chrono::system_clock> log_cycle_start;
	std::chrono::time_point<std::chrono::system_clock> lap_cycle_start;
	std::chrono::nanoseconds elapsed_log_cycle = std::chrono::nanoseconds(0);
	const std::chrono::seconds LOG_CYCLE = std::chrono::seconds(5);
	std::chrono::time_point<std::chrono::system_clock> quick_timer_start_time;
};