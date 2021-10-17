#include "graphics_engine_pool.hpp"

#include "graphics_engine.hpp"
#include "queues.hpp"


GraphicsEnginePool::GraphicsEnginePool(GraphicsEngine& engine) :
	GraphicsEngineBaseModule(engine)
{
	create_command_pool();
	create_descriptor_pool();
}

GraphicsEnginePool::~GraphicsEnginePool()
{
	vkDestroyCommandPool(get_logical_device(), command_pool, nullptr);

	vkDestroyDescriptorPool(get_logical_device(), get_graphics_engine().get_descriptor_pool(), nullptr);
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

void GraphicsEnginePool::create_descriptor_pool()
{
	const auto& engine = get_graphics_engine();

	// 1x uniform descriptor per descriptor set
	VkDescriptorPoolSize uniform_buffer_pool_size{};
	uniform_buffer_pool_size.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	uniform_buffer_pool_size.descriptorCount = GraphicsEngineSwapChain::EXPECTED_NUM_SWAPCHAIN_IMAGES * engine.MAX_NUM_DESCRIPTOR_SETS;

	// 1x texture descriptor per descriptor set
	VkDescriptorPoolSize combined_image_sampler_pool_size{};
	combined_image_sampler_pool_size.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	combined_image_sampler_pool_size.descriptorCount = GraphicsEngineSwapChain::EXPECTED_NUM_SWAPCHAIN_IMAGES * engine.MAX_NUM_DESCRIPTOR_SETS;
	std::vector<VkDescriptorPoolSize> pool_sizes{ uniform_buffer_pool_size, combined_image_sampler_pool_size };

	// allocate a descriptor for every image in the swap chain
	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = pool_sizes.size();
	poolInfo.pPoolSizes = pool_sizes.data();
	// defines maximum number of descriptor sets that may be allocated
	poolInfo.maxSets = GraphicsEngineSwapChain::EXPECTED_NUM_SWAPCHAIN_IMAGES * engine.MAX_NUM_DESCRIPTOR_SETS; // TODO: fix this properly

	if (vkCreateDescriptorPool(get_logical_device(), &poolInfo, nullptr, &descriptor_pool) != VK_SUCCESS)
	{
		throw std::runtime_error("GraphicsEngine::create_descriptor_pool: failed to create descriptor pool!");
	}
}