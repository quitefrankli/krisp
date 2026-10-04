#pragma once

#include "gui_windows.hpp"

#include <array>
#include <optional>
#include <string>
#include <utility>
#include <vector>

struct ImGuiInputTextCallbackData;

class PersistentUiWindow : public EngineUiWindow
{
public:
	using EngineUiWindow::EngineUiWindow;
};

class GuiFPSCounter : public PersistentUiWindow
{
public:
	GuiFPSCounter();
	void process(GameEngine& engine) override;
	void draw() override;

private:
	float fps = 0.0f;
	float tps = 0.0f;
};

class GuiCommandPrompt : public PersistentUiWindow
{
public:
	GuiCommandPrompt();
	void open();
	bool is_open() const { return is_visible(); }
	std::optional<std::string> take_submission();
	void set_output(std::string value) { output = std::move(value); }
	void process(GameEngine& engine) override;
	void draw() override;

private:
	static int input_callback(ImGuiInputTextCallbackData* data);

	struct Suggestion
	{
		std::string name;
		std::string description;
	};

	std::array<char, 256> input_buffer{};
	std::vector<Suggestion> suggestions;
	std::optional<std::string> pending_submission;
	std::string output;
	bool focus_input = false;
	bool just_opened = false;
};
