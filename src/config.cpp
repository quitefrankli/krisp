#include "config.hpp"
#include "utility.hpp"

#include <yaml-cpp/yaml.h>
#include <fmt/color.h>

#include <cassert>


static std::string _project_name;
static YAML::Node config_node;

void Config::init(std::string_view project_name)
{
	if (!config_node.IsNull())
	{
		throw std::runtime_error("Config::init: attempted to reconfigure existing config");
	}
	
	#ifdef _DEBUG
		fmt::print(fg(fmt::color::cyan), "Debug Mode\n");
	#else
		fmt::print(fg(fmt::color::cyan), "Release Mode\n");
	#endif

	_project_name = project_name;
	config_node = YAML::LoadFile(Utility::get_config_path().string());

	if (enable_logging())
	{
		Utility::enable_logging();
	}
}

std::string_view Config::get_project_name()
{
	assert(!config_node.IsNull());
	return _project_name;
}

bool Config::enable_logging()
{
	assert(!config_node.IsNull());
	return config_node["enable_logging"].as<bool>(false);
}

std::pair<int, int> Config::get_window_pos()
{
	assert(!config_node.IsNull());
	return std::make_pair(config_node["window_pos"]["x"].as<int>(10), config_node["window_pos"]["y"].as<int>(-1));
}

bool Config::is_raytracing_enabled()
{
	assert(!config_node.IsNull());
	return config_node["raytracing"].as<bool>(true);
}
