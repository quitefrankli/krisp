#include "graphics_engine_commands.hpp"
#include "engine_base.hpp"
#include "objects/object.hpp"


SpawnObjectCmd::SpawnObjectCmd(const std::shared_ptr<Object>& object_)
{
	object = object_;
}

SpawnObjectCmd::SpawnObjectCmd(Object& object_)
{
	object_ref = &object_;
}

ObjectCommand::ObjectCommand(const Object& object) : object_id(object.get_id())
{
}

void SpawnObjectCmd::process(GraphicsEngineBase* engine)
{
	engine->handle_command(*this);
}

void DeleteObjectCmd::process(GraphicsEngineBase* engine)
{
	engine->handle_command(*this);
}

void StencilObjectCmd::process(GraphicsEngineBase* engine)
{
	engine->handle_command(*this);
}

void UnStencilObjectCmd::process(GraphicsEngineBase* engine)
{
	engine->handle_command(*this);
}

void ShutdownCmd::process(GraphicsEngineBase* engine)
{
	engine->handle_command(*this);
}

void SetRenderModeCmd::process(GraphicsEngineBase* engine)
{
	engine->handle_command(*this);
}

void UpdateCommandBufferCmd::process(GraphicsEngineBase* engine)
{
	engine->handle_command(*this);
}

void UpdateRayTracingCmd::process(GraphicsEngineBase* engine) {
	engine->handle_command(*this);
}

PreviewObjectsCmd::PreviewObjectsCmd(const std::vector<ObjectID>& objects, GuiPhotoBase& gui) :
	objects(objects),
	gui(gui)
{
}

void PreviewObjectsCmd::process(GraphicsEngineBase* engine)
{
	engine->handle_command(*this);
}

void DestroyResourcesCmd::process(GraphicsEngineBase* engine) 
{
	engine->handle_command(*this);
}

UpdateRenderableMaterialsCmd::UpdateRenderableMaterialsCmd(
	const ObjectID object_id,
	const size_t renderable_index,
	const MaterialID diffuse_material,
	const std::optional<MaterialID> normal_material,
	const std::optional<MaterialID> specular_material,
	std::vector<MaterialID> retired_materials) :
	object_id(object_id),
	renderable_index(renderable_index),
	diffuse_material(diffuse_material),
	normal_material(normal_material),
	specular_material(specular_material),
	retired_materials(std::move(retired_materials))
{
}

void UpdateRenderableMaterialsCmd::process(GraphicsEngineBase* engine)
{
	engine->handle_command(*this);
}
