#include "graphics_engine_base_module.hpp"

#include "graphics_engine.hpp"


GraphicsEngineBaseModule::GraphicsEngineBaseModule(GraphicsEngine& engine) :
	graphics_engine(engine)
{
	
}

VkDevice& GraphicsEngineBaseModule::get_logical_device()
{
	return graphics_engine.get_logical_device();
}

uint32_t GraphicsEngineBaseModule::get_num_swap_chains()
{
	return graphics_engine.get_swap_chain().size();
}