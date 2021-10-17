#include "graphics_engine_commands.hpp"
#include "graphics_engine.hpp"

#include <mutex>


void ChangeTextureCmd::process(GraphicsEngine* engine)
{
	engine->change_texture(filename);
}

void ToggleFPSCmd::process(GraphicsEngine* engine)
{

}

void UpdateObjectUniformsCmd::process(GraphicsEngine* engine)
{
	// this can be optimised with a hashmap
	for (auto& obj : engine->get_objects())
	{
		if (obj.get_id() == object_id)
		{
			obj.set_transformation(transformation);
			return;
		}
	}

	throw std::runtime_error("could not find object!");
}