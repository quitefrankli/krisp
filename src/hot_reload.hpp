#pragma once

#include "shared_library.hpp"


class HotReload
{
public:
	static HotReload& get();

	// Builds and loads a new version of the shared library.
	void reload();

	SharedLibFuncPtrs* slfp = nullptr;

private:
	bool generate_new_library(bool throw_on_fail=true);
	bool load_library(bool throw_on_no_runtime_lib=true);
};
