#pragma once

#include "input.hpp"


class Object;
class GameEngine;
class ApplicationUiManager;

class IApplication
{
public:
	virtual ~IApplication() = default;
	// Gameplay that must run before ECS animation/physics processing.
	virtual void on_pre_tick(GameEngine&, float) {}
	virtual void on_tick(GameEngine& engine, float delta) = 0;
	// Gameplay pose adjustments (for example IK) after skeletal animation sampling.
	virtual void on_post_tick(GameEngine&, float) {}
	virtual void on_click(GameEngine& engine, Object& object) = 0;
	virtual void on_mouse_button(GameEngine&, const MouseInput&) {}
	virtual void on_begin(GameEngine& engine) = 0;
	// Called once before the graphics render thread starts. Register all game
	// UI here; the application never owns the ImGui context.
	virtual void create_ui(GameEngine&, ApplicationUiManager&) {}
	// Non-character applications can opt into NORMAL mode without camera
	// follow or player locomotion.
	virtual bool allows_playerless_normal_mode() const { return false; }
	virtual void on_key_press(GameEngine& engine, const KeyInput& key_input) = 0;
	virtual void on_scene_loaded(GameEngine&) {}
};

class DummyApplication : public IApplication
{
public:
	virtual void on_tick(GameEngine&, float) override {}
	virtual void on_click(GameEngine&, Object&) override {}
	virtual void on_begin(GameEngine&) override {}
	virtual void on_key_press(GameEngine&, const KeyInput&) override {}
};
