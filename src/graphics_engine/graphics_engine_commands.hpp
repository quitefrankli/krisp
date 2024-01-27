#pragma once

#include "identifications.hpp"

#include <memory>
#include <vector>


class Object;
class GraphicsEngineBase;
class GuiPhotoBase;

struct GraphicsEngineCommand
{
	virtual void process(GraphicsEngineBase* engine) = 0;
	virtual ~GraphicsEngineCommand() = default;
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
	ObjectCommand(ObjectID object_id) : object_id(object_id) {}
	
	const ObjectID object_id;
};

struct DeleteObjectCmd : public ObjectCommand
{
	DeleteObjectCmd(ObjectID object_id) : ObjectCommand(object_id) {}
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

// DEPRECATED, no longer necessary, acceleration structures are automatically
// rebuilt on rtx toggle
struct UpdateRayTracingCmd : public GraphicsEngineCommand
{
	virtual void process(GraphicsEngineBase* engine) override;
};

struct PreviewObjectsCmd : public GraphicsEngineCommand
{
	PreviewObjectsCmd(const std::vector<Object*>& objects, GuiPhotoBase& gui);
	virtual void process(GraphicsEngineBase* engine) override;

	const std::vector<Object*> objects;
	GuiPhotoBase& gui;
};