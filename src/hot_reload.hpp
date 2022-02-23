#pragma once

#include "shared_library.hpp"


class HotReload
{
public:
	static HotReload& get();

	// reloads dll
	void reload();

private:
	SharedLibFuncPtrs* slfp;
};

extern HotReload hot_reload;