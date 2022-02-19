#include "hot_reload.hpp"

#include <windows.h>

#include <filesystem>
#include <string>
#include <iostream>
#include <regex>


extern std::filesystem::path BINARY_DIRECTORY;
extern std::filesystem::path WORKING_DIRECTORY;

HotReload::func1_t HotReload::func1 = nullptr;
HotReload::func2_t HotReload::func2 = nullptr;
HotReload::func3_t HotReload::func3 = nullptr;

static HMODULE handle = nullptr;

template<typename func_t>
func_t load_func(HMODULE& handle, const char* name)
{
	auto func = (func_t)GetProcAddress(handle, name);
	if (!func)
	{
		std::cerr << "Windows Error: " << GetLastError() << '\n';
		throw std::runtime_error("HotReload: failed to load func1");
	}

	return func;
}

static bool generate_dll()
{
	std::string cmd = "sh " + WORKING_DIRECTORY.parent_path().string() + "/hot_reload.sh";
	return system(cmd.c_str()) == 0;
}

void HotReload::reload()
{
	if (!generate_dll())
	{
		return;
	}

	int max_ver = 0;
	std::filesystem::path library;
	for (auto& p : std::filesystem::directory_iterator(BINARY_DIRECTORY))
	{
		std::string filename = p.path().filename().generic_string();
		if (std::regex_match(filename, std::regex("shared_lib.*dll")))
		{
			if (filename == "shared_lib.dll")
				continue;

			// removes non digits
			std::string output = std::regex_replace(
				filename,
				std::regex("[^0-9]*([0-9]+).*"),
				std::string("$1")
			);
			int ver = std::stoi(output);
			if (ver > max_ver)
			{
				max_ver = ver;
				library = p;
			}
		}
	}

	if (max_ver == 0)
	{
		std::cout << "ERROR: no runtime shared library found!\n";
		return;
	}

	// only works for windows
	if (handle)
		FreeLibrary(handle);
	std::cout << "HotReload: loading " << library.string() << '\n';
	handle = LoadLibrary(library.string().c_str());
	if (!handle)
		throw std::runtime_error("HotReload: invalid handle");
	func1 = load_func<func1_t>(handle, "screen_to_world");
	func2 = load_func<func2_t>(handle, "screen_to_world");
	func3 = load_func<func3_t>(handle, "linear_alg");
}