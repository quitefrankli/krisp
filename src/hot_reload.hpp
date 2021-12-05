#pragma once


class HotReload
{
public:
	using func_ptr = void (*)();

	// reloads dll
	static void reload();

	static func_ptr get_func_ptr() { return func_; }

private:
	static func_ptr func_;
};