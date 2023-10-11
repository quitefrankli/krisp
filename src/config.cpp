#include "config.hpp"

#include <yaml-cpp/yaml.h>

#include <cassert>


struct Pimpl
{
	YAML::Node node;
};
static std::unique_ptr<Pimpl> pimpl;

void Config::initialise_global_config(std::string_view config_file_path)
{
	if (pimpl.get())
	{
		throw std::runtime_error("Config::initialise_global_config: attempted to reconfigure existing config");
	}
	pimpl = std::make_unique<Pimpl>();
	pimpl->node = YAML::LoadFile(config_file_path.data());
}

bool Config::enable_logging()
{
	assert(pimpl.get());
	return pimpl->node["enable_logging"].as<bool>(false);
}

std::pair<int, int> Config::get_window_pos()
{
	assert(pimpl.get());
	return std::make_pair(pimpl->node["window_pos"]["x"].as<int>(10), pimpl->node["window_pos"]["y"].as<int>(-1));
}

bool Config::is_raytracing_enabled()
{
	assert(pimpl.get());
	return pimpl->node["raytracing"].as<bool>(true);
}
