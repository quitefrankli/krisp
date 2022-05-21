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

class SpawnObjectCmd : public GraphicsEngineCommand
{
public:
	SpawnObjectCmd(const std::shared_ptr<Object>& object);
	SpawnObjectCmd(Object& object);

	void process(GraphicsEngine* engine) override;

private:
	std::shared_ptr<Object> object;
	Object* object_ref = nullptr; // used for a GraphicsEngineObjectRef type object
};

class ObjectCommand : public GraphicsEngineCommand
{
public:
	ObjectCommand(const Object& object);
	virtual ~ObjectCommand() = 0 {};

	const uint64_t object_id;
};

class DeleteObjectCmd : public ObjectCommand
{
public:
	DeleteObjectCmd(const Object& object) : ObjectCommand(object) {}

	void process(GraphicsEngine* engine) override;
};

class StencilObjectCmd : public ObjectCommand
{
public:
	StencilObjectCmd(const Object& object) : ObjectCommand(object) {}

	void process(GraphicsEngine* engine) override;
};

class UnStencilObjectCmd : public ObjectCommand
{
public:
	UnStencilObjectCmd(const Object& object) : ObjectCommand(object) {}

	void process(GraphicsEngine* engine) override;
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