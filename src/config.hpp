#pragma once

#include <string>
#include <string_view>
#include <memory>


class Config
{
public:
	Config(std::string_view config_file_path);
	~Config();

	bool enable_logging() const;
	std::pair<int, int> get_window_pos() const;

private:
	struct Pimpl;
	std::unique_ptr<Pimpl> pimpl;
};