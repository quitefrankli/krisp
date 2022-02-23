#pragma once

#include "shared_library.hpp"


class HotReload
{
public:
	static HotReload& get();

	// calls generate_new_dll() followed by load_dll()
	void reload();

	SharedLibFuncPtrs* slfp = nullptr;

private:
	bool generate_new_dll(bool throw_on_fail=true);
	bool load_dll(bool throw_on_no_runtime_lib=true);
};

extern HotReload hot_reload;