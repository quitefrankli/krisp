#pragma once


class Object;

class IApplication
{
public:
	// in seconds
	virtual void on_tick(float delta) = 0;
	virtual void on_click(Object& object) = 0;
	virtual void on_begin() = 0;
	virtual void on_key_press(int key, int scan_code, int action, int mode) = 0;
};

class DummyApplication : public IApplication
{
public:
	// in seconds
	virtual void on_tick(float delta) override {}
	virtual void on_click(Object&) override {}
	virtual void on_begin() override {}
	virtual void on_key_press(int key, int scan_code, int action, int mode) override {};
};