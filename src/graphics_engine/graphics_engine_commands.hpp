#pragma once

#include "objects/object.hpp"

#include <glm/mat4x4.hpp>

#include <string>
#include <memory>


class Object;
class GraphicsEngine;
class GraphicsEngineCommand;

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
	SpawnObjectCmd(std::shared_ptr<Object> object, uint64_t object_id);
	SpawnObjectCmd(Object& object, uint64_t object_id);

	void process(GraphicsEngine* engine) override;

private:
	std::shared_ptr<Object> object;
	Object* object_ref = nullptr; // used for a GraphicsEngineObjectRef type object
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