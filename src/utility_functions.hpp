#pragma once

#include <glm/mat4x4.hpp>

#include <vector>
#include <string>
#include <functional>
#include <stdexcept>


inline std::vector<std::string> char_ptr_arr_to_str_vec(const char** char_arr, int size)
{
	std::vector<std::string> str_vec;
	for (int i = 0; i < size; i++)
	{
		str_vec.emplace_back(char_arr[i]);
	}
	return str_vec;
}

// helper for converting vector of strings to c_style char**
class c_style_str_array
{
	using c_style_str = char*;
	c_style_str* _data;
	size_t num_strs;
	
public:
	c_style_str_array(const std::vector<std::string>& str_vec)
	{
		num_strs = str_vec.size();
		_data = new c_style_str[num_strs];
		for (int i = 0; i < num_strs; i++)
		{
			_data[i] = new char[str_vec[i].size() + 1];
			strcpy_s(_data[i], str_vec[i].size() + 1, str_vec[i].data());
		}
	}

	~c_style_str_array()
	{
		for (int i = 0; i < num_strs; i++)
		{
			delete _data[i];
		}
		delete _data;
	}

	char** data() { 
		if (!_data)
		{
			throw std::runtime_error("ERROR: c_style_str_array has not been initialised properly");
		}
		return _data;
	}
	int size() { return static_cast<int>(num_strs); }
};

class TimerImpl;

class Timer
{
public:
	Timer(const std::string& name = "A");
	~Timer();

	void reset();
	void print_time(const std::string& identifier = "");
	void print_time_us(const std::string& identifier = "");
	int64_t get_elapsed();
	int64_t lap();

private:
	TimerImpl* impl;
	const std::string name_;
};

namespace Helpers {
void print(const ::glm::mat4& mat)
{
	for (int i = 0; i < 4; i++) {
		for (int j = 0; j < 4; j++) {
			int val = mat[i][j] * 1000 / 10;
			if (val >= 0)
				putchar(' ');
			float fval = (float)val / 100.0;
			printf("%.2f, ", fval);
		}
		putchar('\n');
	}
}
}