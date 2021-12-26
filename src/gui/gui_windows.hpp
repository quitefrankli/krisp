#pragma once

#include "maths.hpp"

#include <map>
#include <string>
#include <functional>


class GameEngine;

class GuiWindow
{
public:
	// used in graphics engine
	virtual void draw() = 0;

	// used in game engine
	virtual void process(GameEngine&) {}

	GuiWindow() = default;
	GuiWindow(GuiWindow&&) noexcept = default;
	GuiWindow(const GuiWindow&) = delete;
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

	void process(GameEngine& engine) override;
	void draw() override;

private:
	std::map<std::string, std::function<void(GameEngine&)>> mapping;
	std::function<void(GameEngine&)>* spawning_function = nullptr;

	const float button_width = 120.0f;
	const float button_height = 20.0f;
};