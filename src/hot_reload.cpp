#include "hot_reload.hpp"
#include "utility.hpp"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <iostream>
#include <regex>

#include <dlfcn.h>

static HotReload hot_reload;
static void* handle = nullptr;

HotReload& HotReload::get() 
{ 
	// hasn't been loaded yet
	if (!hot_reload.slfp)
	{
		if (!hot_reload.load_library(false))
		{
			std::cout << "HotReload: no shared library found, attempting to generate one...\n";
			hot_reload.reload();
		}
	}

	return hot_reload; 
}

void HotReload::reload()
{
	generate_new_library();
	load_library();
}

bool HotReload::generate_new_library(bool throw_on_fail)
{
	const auto& build_path = Utility::get_build_path();
	const auto& binary_path = Utility::get_binary_path();
	const std::string cmd = "meson compile -C \"" + build_path.string()
		+ "\" -j 6 shared_lib";
	if (system(cmd.c_str()) != 0)
	{
		if (throw_on_fail)
		{
			throw std::runtime_error("HotReload: shared library build failed");
		}
		return false;
	}

	const auto source_library = build_path / "shared_lib/libshared_lib.so";
	if (!std::filesystem::exists(source_library))
	{
		if (throw_on_fail)
			throw std::runtime_error("HotReload: built shared library not found");
		return false;
	}

	std::filesystem::create_directories(binary_path);
	int next_version = 1;
	for (const auto& entry : std::filesystem::directory_iterator(binary_path))
	{
		const std::string filename = entry.path().filename().string();
		if (!std::regex_match(filename, std::regex("libshared_lib[0-9]+\\.so")))
			continue;
		const int version = std::stoi(std::regex_replace(
			filename, std::regex("[^0-9]*([0-9]+).*"), "$1"));
		next_version = std::max(next_version, version + 1);
	}

	const auto versioned_library = binary_path
		/ ("libshared_lib" + std::to_string(next_version) + ".so");
	std::filesystem::copy_file(source_library, versioned_library);
	std::cout << "HotReload: created " << versioned_library.string() << '\n';
	return true;
}

bool HotReload::load_library(bool throw_on_no_runtime_lib)
{
	int max_ver = 0;
	std::filesystem::path library;
	for (auto& p : std::filesystem::directory_iterator(Utility::get_binary_path()))
	{
		std::string filename = p.path().filename().generic_string();
		if (std::regex_match(filename, std::regex("libshared_lib[0-9]+\\.so")))
		{
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
		if (throw_on_no_runtime_lib)
		{
			throw std::runtime_error("ERROR: no runtime shared library found!");
		}
		return false;
	}

	SharedLibFuncPtrs* new_slfp = nullptr;
	std::cout << "HotReload: loading " << library.string() << '\n';
	void* new_handle = dlopen(library.c_str(), RTLD_NOW | RTLD_LOCAL);
	if (!new_handle)
		throw std::runtime_error(std::string("HotReload: ") + dlerror());
	dlerror();
	new_slfp = reinterpret_cast<SharedLibFuncPtrs*>(dlsym(new_handle, "shared_lib_func_ptrs"));
	if (const char* error = dlerror())
	{
		dlclose(new_handle);
		throw std::runtime_error(std::string("HotReload: ") + error);
	}
	if (handle)
		dlclose(handle);
	handle = new_handle;
	if (!new_slfp)
	{
		std::cout << "HotReload: load error\n";
		return false;
	}
	slfp = new_slfp;

	slfp->load_check();

	return true;
}
