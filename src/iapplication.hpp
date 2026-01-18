#pragma once

#include "input.hpp"


class Object;

class IApplication
{
public:
	// in seconds
	virtual void on_tick(float delta) = 0;
	virtual void on_click(Object& object) = 0;
	virtual void on_begin() = 0;
	virtual void on_key_press(const KeyInput& key_input) = 0;
};

class DummyApplication : public IApplication
{
public:
	// in seconds
	virtual void on_tick(float delta) override {}
	virtual void on_click(Object&) override {}
	virtual void on_begin() override {}
	virtual void on_key_press(const KeyInput& key_input) override {};
};