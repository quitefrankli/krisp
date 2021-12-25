#pragma once

#include "maths.hpp"

#include <map>
#include <string>


class GuiWindow
{
public:
	// used in graphics engine
	virtual void draw() = 0;

	// // used in game engine, TODO add this
	// virtual void process() = 0;
};

class GuiGraphicsSettings : public GuiWindow
{
public:
	GuiGraphicsSettings();

	void draw() override;

public:
	float light_strength = 1.0f;
	Maths::Ray light_ray;	
};

class GuiObjectSpawner : public GuiWindow
{
public:
	GuiObjectSpawner();

	void draw() override;
	std::map<std::string, bool> mapping;

private:
	const float button_width = 120.0f;
	const float button_height = 20.0f;
};