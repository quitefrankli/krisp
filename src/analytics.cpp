#include "analytics.hpp"
#include "utility.hpp"

#include <quill/LogMacros.h>

#include <algorithm>
#include <chrono>
#include <cmath>


using namespace std::chrono;

Analytics::Analytics(std::string text_, const int period) :
	text(std::move(text_)),
	LOG_PERIOD(period),
	CALLBACK_PERIOD(period)
{
}

Analytics::Analytics(
	std::string text_,
	std::function<void(float)>&& on_log,
	const int callback_period,
	const int log_period) :
	text(std::move(text_)),
	LOG_PERIOD(log_period),
	CALLBACK_PERIOD(callback_period),
	on_log_period(std::move(on_log))
{
}

void Analytics::Statistics::add(const double sample)
{
	++count;
	const double delta = sample - mean;
	mean += delta / static_cast<double>(count);
	squared_deviation_sum += delta * (sample - mean);
	min = std::min(min, sample);
	max = std::max(max, sample);
}

void Analytics::Statistics::reset()
{
	*this = {};
}

double Analytics::Statistics::standard_deviation() const
{
	return count == 0 ? 0.0
		: std::sqrt(squared_deviation_sum / static_cast<double>(count));
}

void Analytics::start()
{
	assert(state != State::STARTED);
	lap_cycle_start = system_clock::now();
	if (state == State::FRESH)
	{
		log_cycle_start = lap_cycle_start;
		callback_cycle_start = lap_cycle_start;
	}
	state = State::STARTED;
}

void Analytics::stop()
{
	assert(state == State::STARTED);
	state = State::STOPPED;
	auto now = system_clock::now();
	const double sample = duration<double, std::micro>(now - lap_cycle_start).count();
	statistics.add(sample);
	if (on_log_period)
		callback_statistics.add(sample);
	if (on_log_period && now - callback_cycle_start > CALLBACK_PERIOD)
	{
		on_log_period.value()(static_cast<float>(callback_statistics.average()));
		callback_cycle_start = now;
		callback_statistics.reset();
	}
	if (now - log_cycle_start > LOG_PERIOD)
	{
		constexpr double microseconds_per_millisecond = 1000.0;
		const double average = statistics.average() / microseconds_per_millisecond;
		LOG_INFO(Utility::get_logger(),
			"{} avg {:.2f} ms, std dev {:.2f} ms, min {:.2f} ms, max {:.2f} ms",
			text, average,
			statistics.standard_deviation() / microseconds_per_millisecond,
			statistics.minimum() / microseconds_per_millisecond,
			statistics.maximum() / microseconds_per_millisecond);
		log_cycle_start = now;
		statistics.reset();
	}
}

void Analytics::quick_timer_start()
{
	quick_timer_start_time = system_clock::now();
}

void Analytics::quick_timer_stop()
{
	quick_timer_stop("");
}

void Analytics::quick_timer_stop(const std::string& mesg)
{
	const auto elapsed = duration_cast<nanoseconds>(system_clock::now() - quick_timer_start_time);
	const double elapsed_float = std::round((double)elapsed.count() / 10.0) / 100.0;
	LOG_INFO(Utility::get_logger(), "{}, quick timer {} microseconds", mesg, elapsed_float);
}
