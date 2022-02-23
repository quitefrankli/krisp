#include "hot_reload.hpp"

#include <windows.h>

#include <filesystem>
#include <string>
#include <iostream>
#include <regex>


extern std::filesystem::path BINARY_DIRECTORY;
extern std::filesystem::path WORKING_DIRECTORY;

static HotReload hot_reload;
static HMODULE handle = nullptr;


HotReload& HotReload::get() { return hot_reload; }

void HotReload::reload()
{
	std::string cmd = "sh " + WORKING_DIRECTORY.parent_path().string() + "/hot_reload.sh";
	if (system(cmd.c_str()) != 0)
	{
		throw std::runtime_error("HotReload: script failed");
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

	slfp = (SharedLibFuncPtrs*)GetProcAddress(handle, "shared_lib_func_ptrs");
	if (!slfp)
	{
		std::cout << "HotReload: load error\n";
		return;
	}

	slfp->load_check();
}