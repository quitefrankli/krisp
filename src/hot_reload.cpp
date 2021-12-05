#include "hot_reload.hpp"

#include <windows.h>

#include <filesystem>
#include <string>
#include <iostream>
#include <regex>


extern std::string RELATIVE_BINARY_PATH;

HotReload::func_ptr HotReload::func_ = nullptr;

static HMODULE handle = nullptr;

void HotReload::reload()
{
	std::filesystem::path path(RELATIVE_BINARY_PATH);
	path = path.parent_path();
	int max_ver = 0;
	std::filesystem::path library;
	for (auto& p : std::filesystem::directory_iterator(path))
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
	handle = LoadLibrary(library.string().c_str());
	func_ = (func_ptr)GetProcAddress(handle, "foo");
	func_();
}