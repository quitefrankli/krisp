#pragma once


class HotReload
{
public:
	using func_ptr = void (*)();

	// reloads dll
	void reload();

	func_ptr get_func_ptr() const { return func_; }

private:
	func_ptr func_ = nullptr;
};