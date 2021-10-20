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
		LOG_INFO(logger, "{} {} microseconds", text, (double)elapsed_log_cycle.count() / (double)num_elapsed_cycles / 1e3);
		log_cycle_start = system_clock::now();
		num_elapsed_cycles = 0;
		elapsed_log_cycle = nanoseconds(0);
	}
}