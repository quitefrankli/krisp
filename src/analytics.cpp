#include "analytics.hpp"

#include <quill/Quill.h>

#include <chrono>


extern quill::Logger* logger;

using namespace std::chrono;

void Analytics::start()
{
	lap_cycle_start = system_clock::now();
	if (!has_started)
	{
		log_cycle_start = lap_cycle_start;
		has_started = true;
	}
}

void Analytics::stop()
{
	elapsed_log_cycle += duration_cast<nanoseconds>(system_clock::now() - lap_cycle_start);
	num_elapsed_cycles++;
	if (system_clock::now() - log_cycle_start > LOG_CYCLE)
	{
		double elapsed_float = round((double)elapsed_log_cycle.count() / (double)num_elapsed_cycles / 10.0) / 100.0; 
		LOG_INFO(logger, "{} {} microseconds", text, elapsed_float);
		log_cycle_start = system_clock::now();
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
	LOG_INFO(logger, "{}, quick timer {} microseconds", mesg, elapsed_float);
}