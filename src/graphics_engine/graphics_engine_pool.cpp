#include "graphics_engine_pool.hpp"

#include "graphics_engine.hpp"
#include "queues.hpp"


GraphicsEnginePool::GraphicsEnginePool(GraphicsEngine& engine) :
	GraphicsEngineBaseModule(engine)
{
	create_command_pool();
	create_descriptor_set_layout();
	create_descriptor_pool();
}

GraphicsEnginePool::~GraphicsEnginePool()
{
	vkDestroyCommandPool(get_logical_device(), command_pool, nullptr);

	vkDestroyDescriptorPool(get_logical_device(), get_graphics_engine().get_descriptor_pool(), nullptr);

	vkDestroyDescriptorSetLayout(get_logical_device(), descriptor_set_layout, nullptr);
}

void GraphicsEnginePool::create_command_pool()
{
	QueueFamilyIndices queue_family_indices = get_graphics_engine().findQueueFamilies(get_physical_device());
	VkCommandPoolCreateInfo command_pool_create_info{};
	command_pool_create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	command_pool_create_info.queueFamilyIndex = queue_family_indices.graphicsFamily.value();
	command_pool_create_info.flags = VkCommandPoolCreateFlagBits::VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

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
	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = pool_sizes.size();
	poolInfo.pPoolSizes = pool_sizes.data();
	// defines maximum number of descriptor sets that may be allocated
	poolInfo.maxSets = GraphicsEngineSwapChain::EXPECTED_NUM_SWAPCHAIN_IMAGES * engine.MAX_NUM_DESCRIPTOR_SETS; // TODO: fix this properly

	//
	// below may be necessary for ImGui
	//

	// pool_sizes =
	// {
	// 	{ VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
	// 	{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
	// 	{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
	// 	{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
	// 	{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
	// 	{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
	// 	{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
	// 	{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
	// 	{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
	// 	{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
	// 	{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
	// };
	// // poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	// poolInfo.maxSets = 10000 * pool_sizes.size();
	// poolInfo.poolSizeCount = (uint32_t)pool_sizes.size();
	// poolInfo.pPoolSizes = pool_sizes.data();
	
	if (vkCreateDescriptorPool(get_logical_device(), &poolInfo, nullptr, &descriptor_pool) != VK_SUCCESS)
	{
		throw std::runtime_error("GraphicsEngine::create_descriptor_pool: failed to create descriptor pool!");
	}
}

void GraphicsEnginePool::create_descriptor_set_layout()
{
	VkDescriptorSetLayoutBinding ubo_layout_binding{};
	ubo_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	ubo_layout_binding.binding = 0; // this must be synced with the one in the shaders
	ubo_layout_binding.descriptorCount = 1;
	ubo_layout_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT; // defines which shader stage the descriptor is going to be referenced
	ubo_layout_binding.pImmutableSamplers = nullptr; // only relevant for image sampling related descriptors

	VkDescriptorSetLayoutBinding sampler_layout_binding{};
	sampler_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	sampler_layout_binding.binding = 1; // this must be synced with the one in the shaders
	sampler_layout_binding.descriptorCount = 1;
	sampler_layout_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT; // defines which shader stage the descriptor is going to be referenced
	sampler_layout_binding.pImmutableSamplers = nullptr; // only relevant for image sampling related descriptors

	std::vector<VkDescriptorSetLayoutBinding> bindings{ ubo_layout_binding, sampler_layout_binding };
	VkDescriptorSetLayoutCreateInfo layout_info{};
	layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layout_info.bindingCount = static_cast<uint32_t>(bindings.size());
	layout_info.pBindings = bindings.data();

	if (vkCreateDescriptorSetLayout(get_logical_device(), &layout_info, nullptr, &descriptor_set_layout) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create descriptor set layout!");
	}
}