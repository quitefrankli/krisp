#pragma once

#include "identifications.hpp"
#include "renderable/render_types.hpp"
#include <memory>
#include <vector>


class GraphicsEngineBase;
class GuiPhotoBase;

struct GraphicsEngineCommand
{
	virtual void process(GraphicsEngineBase* engine) = 0;
	virtual ~GraphicsEngineCommand() = default;
};

struct ObjectCommand : public GraphicsEngineCommand
{
	ObjectCommand(ObjectID object_id) : object_id(object_id) {}
	
	const ObjectID object_id;
};

struct StencilObjectCmd : public ObjectCommand
{
	using ObjectCommand::ObjectCommand;
	virtual void process(GraphicsEngineBase* engine) override;
};

struct UnStencilObjectCmd : public ObjectCommand
{
	using ObjectCommand::ObjectCommand;
	virtual void process(GraphicsEngineBase* engine) override;
};

struct ShutdownCmd : public GraphicsEngineCommand
{
	virtual void process(GraphicsEngineBase* engine) override;
};

struct SetRenderModeCmd : public GraphicsEngineCommand
{
	explicit SetRenderModeCmd(ERenderMode render_mode) : render_mode(render_mode) {}
	virtual void process(GraphicsEngineBase* engine) override;

	ERenderMode render_mode;
};

struct PreviewObjectsCmd : public GraphicsEngineCommand
{
	PreviewObjectsCmd(const std::vector<ObjectID>& objects, GuiPhotoBase& gui);
	virtual void process(GraphicsEngineBase* engine) override;

	const std::vector<ObjectID> objects;
	GuiPhotoBase& gui;
};
