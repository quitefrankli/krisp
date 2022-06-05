#pragma once

#include <memory>


class Object;
class GraphicsEngineBase;

struct GraphicsEngineCommand
{
	virtual void process(GraphicsEngineBase* engine) = 0;
};

struct SpawnObjectCmd : public GraphicsEngineCommand
{
	SpawnObjectCmd(const std::shared_ptr<Object>& object);
	SpawnObjectCmd(Object& object);
	virtual void process(GraphicsEngineBase* engine) override;

	std::shared_ptr<Object> object;
	Object* object_ref = nullptr; // used for a GraphicsEngineObjectRef type object
};

struct ObjectCommand : public GraphicsEngineCommand
{
	ObjectCommand(const Object& object);
	
	const uint64_t object_id;
};

struct DeleteObjectCmd : public ObjectCommand
{
	DeleteObjectCmd(const Object& object) : ObjectCommand(object) {}
	virtual void process(GraphicsEngineBase* engine) override;
};

struct StencilObjectCmd : public ObjectCommand
{
	StencilObjectCmd(const Object& object) : ObjectCommand(object) {}
	virtual void process(GraphicsEngineBase* engine) override;
};

struct UnStencilObjectCmd : public ObjectCommand
{
	UnStencilObjectCmd(const Object& object) : ObjectCommand(object) {}
	virtual void process(GraphicsEngineBase* engine) override;
};

struct ShutdownCmd : public GraphicsEngineCommand
{
	virtual void process(GraphicsEngineBase* engine) override;
};

struct ToggleWireFrameModeCmd : public GraphicsEngineCommand
{
	virtual void process(GraphicsEngineBase* engine) override;
};

struct UpdateCommandBufferCmd : public GraphicsEngineCommand
{
	virtual void process(GraphicsEngineBase* engine) override;
};