#pragma once

#include <string>
#include <chrono>
#include <functional>
#include <limits>
#include <optional>


class Analytics
{
public:
	class Statistics
	{
	public:
		void add(double sample);
		void reset();
		double average() const { return mean; }
		double standard_deviation() const;
		double minimum() const { return count == 0 ? 0.0 : min; }
		double maximum() const { return count == 0 ? 0.0 : max; }

	private:
		uint64_t count = 0;
		double mean = 0.0;
		double squared_deviation_sum = 0.0;
		double min = std::numeric_limits<double>::max();
		double max = std::numeric_limits<double>::lowest();
	};

	// period in seconds for logging to occur
	Analytics(std::string text, int period = 5);

	Analytics(
		std::string text,
		std::function<void(float)>&& on_log,
		int callback_period = 1,
		int log_period = 5);

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
	const std::string text;

private:
	std::chrono::time_point<std::chrono::system_clock> log_cycle_start;
	std::chrono::time_point<std::chrono::system_clock> callback_cycle_start;
	std::chrono::time_point<std::chrono::system_clock> lap_cycle_start;
	Statistics statistics;
	Statistics callback_statistics;
	std::chrono::time_point<std::chrono::system_clock> quick_timer_start_time;
	// once every X seconds Analytics::stop is called, data will be logged
	const std::chrono::seconds LOG_PERIOD;
	const std::chrono::seconds CALLBACK_PERIOD;

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
