#pragma once


class Object;

class IApplication
{
public:
	
	virtual void on_tick(float delta) = 0;
	virtual void on_click(Object& object) = 0;
	virtual void on_begin() = 0;
};

class DummyApplication : public IApplication
{
public:
	virtual void on_tick(float delta) {}
	virtual void on_click(Object&) {}
	virtual void on_begin() {}
};