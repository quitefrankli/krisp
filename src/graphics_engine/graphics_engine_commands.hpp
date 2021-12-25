#pragma once

#include "objects.hpp"

#include <glm/mat4x4.hpp>

#include <string>
#include <memory>


class Object;
class GraphicsEngine;
class GraphicsEngineCommand;
class GuiWindow;

class GraphicsEngineCommand
{
public:
	virtual void process(GraphicsEngine* engine) = 0;
};

class ToggleFPSCmd : public GraphicsEngineCommand
{
public:
	void process(GraphicsEngine* engine) override;
};

class ObjectCommand : public GraphicsEngineCommand
{
public:
	uint64_t object_id;
};

class SpawnObjectCmd : public ObjectCommand
{
public:
	void process(GraphicsEngine* engine) override;

	std::shared_ptr<Object> object;
};

class SpawnGuiCmd : public GraphicsEngineCommand
{
public:
	void process(GraphicsEngine* engine) override;

	std::shared_ptr<GuiWindow> gui;
};

class ShutdownCmd : public GraphicsEngineCommand
{
public:
	void process(GraphicsEngine* engine) override;
};

class ToggleWireFrameModeCmd : public GraphicsEngineCommand
{
public:
	void process(GraphicsEngine* engine) override;
};

class UpdateCommandBufferCmd : public GraphicsEngineCommand
{
public:
	void process(GraphicsEngine* engine) override;
};