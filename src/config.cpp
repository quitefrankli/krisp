#include "config.hpp"

#include <yaml-cpp/yaml.h>


struct Config::Pimpl
{
	YAML::Node node;
};

Config::Config(std::string_view config_file_path) :
	pimpl(std::make_unique<Pimpl>())
{
	pimpl->node = YAML::LoadFile(config_file_path.data());
}

Config::~Config() = default;

bool Config::enable_logging() const
{
	return pimpl->node["enable_logging"].as<bool>(false);
}

std::pair<int, int> Config::get_window_pos() const
{
	return std::make_pair(pimpl->node["window_pos"]["x"].as<int>(10), pimpl->node["window_pos"]["y"].as<int>(-1));
}