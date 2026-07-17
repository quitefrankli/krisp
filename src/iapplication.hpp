#pragma once

#include "input.hpp"


class Object;
class GameEngine;

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
	virtual void on_begin(GameEngine& engine) = 0;
	virtual void on_key_press(GameEngine& engine, const KeyInput& key_input) = 0;
};

class DummyApplication : public IApplication
{
public:
	virtual void on_tick(GameEngine&, float) override {}
	virtual void on_click(GameEngine&, Object&) override {}
	virtual void on_begin(GameEngine&) override {}
	virtual void on_key_press(GameEngine&, const KeyInput&) override {}
};
