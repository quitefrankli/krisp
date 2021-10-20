#include "utility_functions.hpp"

#include <iostream>
#include <thread>
#include <chrono>


class TimerImpl
{
public:
	std::chrono::time_point<std::chrono::system_clock> start;
};

Timer::Timer(const std::string& name) : name_(name)
{
	impl = new TimerImpl();
	impl->start = std::chrono::system_clock::now();
}

Timer::~Timer()
{
	delete impl;
}

void Timer::reset()
{
	impl->start = std::chrono::system_clock::now();
}

void Timer::print_time(const std::string& identifier)
{
	auto duration = std::chrono::system_clock::now() - impl->start;
	std::cout << "Timer - " << name_ << " (" << identifier << "): " <<
		std::chrono::duration_cast<std::chrono::milliseconds>(duration).count() << "ms\n";
}

void Timer::print_time_us(const std::string& identifier)
{
	auto duration = std::chrono::system_clock::now() - impl->start;
	std::cout << "Timer - " << name_ << " (" << identifier << "): " <<
		std::chrono::duration_cast<std::chrono::microseconds>(duration).count() << "microsecondss\n";
}

int64_t Timer::get_elapsed()
{
	auto duration = std::chrono::system_clock::now() - impl->start;
	return std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
}

int64_t Timer::lap()
{
	auto now = std::chrono::system_clock::now();
	auto time = std::chrono::duration_cast<std::chrono::microseconds>(now - impl->start).count();
	impl->start = now;
	return time;
}