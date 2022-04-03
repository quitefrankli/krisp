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

	virtual void draw() override;

public:
	float light_strength = 1.0f;
	Maths::Ray light_ray;	
};

class GuiObjectSpawner : public GuiWindow
{
public:
	GuiObjectSpawner();

	virtual void process(GameEngine& engine) override;
	virtual void draw() override;

public:
	bool use_texture = false;

private:
	const std::string texture_path = "../resources/textures/texture.jpg";

	using spawning_function_type = std::function<void(GameEngine&, bool)>;
	std::map<std::string, spawning_function_type> mapping;
	spawning_function_type* spawning_function = nullptr;

	const float button_width = 120.0f;
	const float button_height = 20.0f;
};

class ImFont;
class GuiFPSCounter : public GuiWindow
{
public:
	GuiFPSCounter(unsigned initial_window_width);

	virtual void draw() override;

	float fps = 0.0f;
	unsigned window_width;

private:
	ImFont* font = nullptr;
};