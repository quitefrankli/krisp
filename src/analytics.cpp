#include "analytics.hpp"
#include "utility.hpp"

#include <quill/Quill.h>

#include <chrono>


using namespace std::chrono;

Analytics::Analytics(const int period) : LOG_PERIOD(period)
{
}

Analytics::Analytics(std::function<void(float)>&& on_log, const int period) :
	on_log_period(std::move(on_log)), LOG_PERIOD(period)
{
}

void Analytics::start()
{
	assert(state != State::STARTED);
	lap_cycle_start = system_clock::now();
	if (state == State::FRESH)
	{
		log_cycle_start = lap_cycle_start;
	}
	state = State::STARTED;
}

void Analytics::stop()
{
	assert(state == State::STARTED);
	state = State::STOPPED;
	auto now = system_clock::now();
	elapsed_log_cycle += duration_cast<nanoseconds>(now - lap_cycle_start);
	num_elapsed_cycles++;
	if (now - log_cycle_start > LOG_PERIOD)
	{
		const float avg_float = (float)elapsed_log_cycle.count() / (float)num_elapsed_cycles / 1e3;
		if (on_log_period)
		{
			on_log_period.value()(avg_float);
		} else
		{
			LOG_INFO(Utility::get_logger(), "{} {:.2f} microseconds", text, avg_float);
		}
		log_cycle_start = now;
		num_elapsed_cycles = 0;
		elapsed_log_cycle = nanoseconds(0);
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
	auto elapsed = duration_cast<nanoseconds>(system_clock::now() - quick_timer_start_time);
	double elapsed_float =  round((double)elapsed.count() / 10.0) / 100.0;
	LOG_INFO(Utility::get_logger(), "{}, quick timer {} microseconds", mesg, elapsed_float);
}