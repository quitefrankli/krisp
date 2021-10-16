#include "graphics_engine_pool.hpp"

#include "graphics_engine.hpp"
#include "queues.hpp"


GraphicsEnginePool::GraphicsEnginePool(GraphicsEngine& engine) :
	GraphicsEngineBaseModule(engine)
{
	create_command_pool();
}

GraphicsEnginePool::~GraphicsEnginePool()
{
	vkDestroyCommandPool(get_logical_device(), command_pool, nullptr);
}

void GraphicsEnginePool::create_command_pool()
{
	QueueFamilyIndices queue_family_indices = get_graphics_engine().findQueueFamilies(get_physical_device());
	VkCommandPoolCreateInfo command_pool_create_info{};
	command_pool_create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	command_pool_create_info.queueFamilyIndex = queue_family_indices.graphicsFamily.value();
	command_pool_create_info.flags = 0;

	if (vkCreateCommandPool(get_logical_device(), &command_pool_create_info, nullptr, &command_pool) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create command pool!");
	}
}