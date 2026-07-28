#pragma once

#include "gui_windows.hpp"

#include <array>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

// A small declarative palette. The host translates it to ImGui style values,
// keeping application code free of ImGui context ownership.
struct ApplicationUiTheme
{
	std::array<float, 4> text{1.0f, 1.0f, 1.0f, 1.0f};
	std::array<float, 4> window_background{0.08f, 0.09f, 0.11f, 0.94f};
	std::array<float, 4> accent{0.20f, 0.55f, 0.95f, 1.0f};
	float window_rounding = 6.0f;
	float window_border_size = 1.0f;
};

enum class ApplicationUiAnchor
{
	TOP_LEFT,
	TOP_RIGHT,
	BOTTOM_LEFT,
	BOTTOM_RIGHT,
	CENTER
};

struct ApplicationUiLayout
{
	ApplicationUiAnchor anchor = ApplicationUiAnchor::TOP_LEFT;
	std::array<float, 2> offset{0.0f, 0.0f};
	std::array<float, 2> size{0.0f, 0.0f}; // zero keeps the ImGui-calculated axis size
};

class ApplicationUiElement : public GuiWindow
{
public:
	using GuiWindow::GuiWindow;
	void draw() final;

protected:
	virtual void draw_contents() = 0;
	virtual int window_flags() const = 0;
};

class ApplicationUiWindow : public ApplicationUiElement
{
public:
	using ApplicationUiElement::ApplicationUiElement;

protected:
	int window_flags() const final;
};

class ApplicationUiOverlay : public ApplicationUiElement
{
public:
	using ApplicationUiElement::ApplicationUiElement;

protected:
	int window_flags() const final;
};

class ApplicationUiManager
{
public:
	template<typename GuiT, typename... Args>
	GuiT& register_window(ApplicationUiLayout layout, Args&&... args)
	{
		static_assert(std::is_base_of_v<ApplicationUiWindow, GuiT>);
		return add<GuiT>(layout, false, std::forward<Args>(args)...);
	}

	template<typename GuiT, typename... Args>
	GuiT& register_overlay(ApplicationUiLayout layout, Args&&... args)
	{
		static_assert(std::is_base_of_v<ApplicationUiOverlay, GuiT>);
		return add<GuiT>(layout, true, std::forward<Args>(args)...);
	}

	void set_theme(ApplicationUiTheme value)
	{
		if (!accepting_registrations)
			throw std::logic_error("Application UI registration is closed");
		theme = std::move(value);
	}
	const ApplicationUiTheme& get_theme() const { return theme; }
	void seal() { accepting_registrations = false; }
	bool is_sealed() const { return !accepting_registrations; }

	const std::vector<std::unique_ptr<GuiWindow>>& get_windows() const { return windows; }
	bool is_overlay(size_t index) const { return overlays[index]; }
	const ApplicationUiLayout& get_layout(size_t index) const { return layouts[index]; }

	void process(GameEngine& engine)
	{
		for (auto& window : windows)
			if (window->is_visible()) window->process(engine);
	}

private:
	template<typename GuiT, typename... Args>
	GuiT& add(ApplicationUiLayout layout, bool overlay, Args&&... args)
	{
		if (!accepting_registrations)
			throw std::logic_error("Application UI registration is closed");
		windows.push_back(std::make_unique<GuiT>(std::forward<Args>(args)...));
		layouts.push_back(std::move(layout));
		overlays.push_back(overlay);
		return *static_cast<GuiT*>(windows.back().get());
	}

	ApplicationUiTheme theme;
	std::vector<std::unique_ptr<GuiWindow>> windows;
	std::vector<ApplicationUiLayout> layouts;
	std::vector<bool> overlays;
	bool accepting_registrations = true;
};
