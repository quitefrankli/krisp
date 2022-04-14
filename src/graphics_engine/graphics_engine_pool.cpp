#include "graphics_engine_pool.hpp"

#include "graphics_engine.hpp"
#include "queues.hpp"
#include "uniform_buffer_object.hpp"

#include <fmt/core.h>


GraphicsEnginePool::GraphicsEnginePool(GraphicsEngine& engine) :
	GraphicsEngineBaseModule(engine),
	high_freq_descriptor_set_layout(descriptor_set_layouts[0]),
	low_freq_descriptor_set_layout(descriptor_set_layouts[1])
{
	create_command_pool();
	create_descriptor_set_layout();
	create_descriptor_pool();

	// global uniform buffer
	create_buffer(sizeof(GlobalUniformBufferObject),
				  VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
				  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				  global_uniform_buffer,
				  global_uniform_buffer_memory);
	allocate_descriptor_set();
}

GraphicsEnginePool::~GraphicsEnginePool()
{
	vkDestroyCommandPool(get_logical_device(), command_pool, nullptr);
	vkDestroyDescriptorPool(get_logical_device(), descriptor_pool, nullptr);
	for (auto& layout : descriptor_set_layouts)
	{
		vkDestroyDescriptorSetLayout(get_logical_device(), layout, nullptr);
	}

	vkDestroyBuffer(get_logical_device(), global_uniform_buffer, nullptr);
	vkFreeMemory(get_logical_device(), global_uniform_buffer_memory, nullptr);
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

	VkDescriptorPoolSize uniform_buffer_pool_size{};
	uniform_buffer_pool_size.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	// max number of uniform buffers per descriptor set
	uniform_buffer_pool_size.descriptorCount = MAX_UNIFORMS_PER_DESCRIPTOR_SET;

	VkDescriptorPoolSize combined_image_sampler_pool_size{};
	combined_image_sampler_pool_size.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	// max number of combined image samplers per descriptor set
	combined_image_sampler_pool_size.descriptorCount = MAX_COMBINED_IMAGE_SAMPLERS_PER_DESCRIPTOR_SET;

	std::vector<VkDescriptorPoolSize> pool_sizes{ uniform_buffer_pool_size, combined_image_sampler_pool_size };
	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = pool_sizes.size();
	poolInfo.pPoolSizes = pool_sizes.data();
	// defines maximum number of descriptor sets that may be allocated
	poolInfo.maxSets = get_max_descriptor_sets();

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

	fmt::print("GraphicsEnginePool::create_descriptor_pool: max_sets:={}\n"
		"\tper_set_max_uniform_buffer_count:={}\n"
		"\tper_set_max_combined_image_sampler_count:={}\n",
		poolInfo.maxSets, uniform_buffer_pool_size.descriptorCount, combined_image_sampler_pool_size.descriptorCount);

	if (vkCreateDescriptorPool(get_logical_device(), &poolInfo, nullptr, &descriptor_pool) != VK_SUCCESS)
	{
		throw std::runtime_error("GraphicsEngine::create_descriptor_pool: failed to create descriptor pool!");
	}
}

void GraphicsEnginePool::create_descriptor_set_layout()
{
	auto create_layout = [this](std::vector<VkDescriptorSetLayoutBinding>& bindings, VkDescriptorSetLayout* layout)
	{
		VkDescriptorSetLayoutCreateInfo layout_info{};
		layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layout_info.bindingCount = static_cast<uint32_t>(bindings.size());
		layout_info.pBindings = bindings.data();
			
		if (vkCreateDescriptorSetLayout(get_logical_device(), &layout_info, nullptr, layout) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create descriptor set layout!");
		}
	};

	// Model + Texture
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
		create_layout(bindings, &high_freq_descriptor_set_layout);
	}

	// global uniforms i.e. camera & lighting
	{
		VkDescriptorSetLayoutBinding gubo_layout_binding{};
		gubo_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		gubo_layout_binding.binding = 0; // this must be synced with the one in the shaders
		gubo_layout_binding.descriptorCount = 1;
		gubo_layout_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT; // defines which shader stage the descriptor is going to be referenced
		gubo_layout_binding.pImmutableSamplers = nullptr; // only relevant for image sampling related descriptors

		std::vector<VkDescriptorSetLayoutBinding> bindings{ gubo_layout_binding };
		create_layout(bindings, &low_freq_descriptor_set_layout);
	}
}

void GraphicsEnginePool::allocate_descriptor_set()
{
	// allocation + writing for low frequency descriptor sets
	const int MAX_LOW_FREQ_DESCRIPTOR_SETS = 1;
	{
		VkDescriptorSetAllocateInfo alloc_info{};
		alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		alloc_info.descriptorPool = descriptor_pool;
		alloc_info.descriptorSetCount = MAX_LOW_FREQ_DESCRIPTOR_SETS;
		alloc_info.pSetLayouts = &low_freq_descriptor_set_layout;

		if (vkAllocateDescriptorSets(get_logical_device(), &alloc_info, &global_descriptor_set) != VK_SUCCESS)
			throw std::runtime_error("GraphicsEnginePool: failed to allocate descriptor sets!");

		VkDescriptorBufferInfo buffer_info{};
		buffer_info.buffer = global_uniform_buffer;
		buffer_info.offset = 0;
		buffer_info.range = sizeof(GlobalUniformBufferObject);

		VkWriteDescriptorSet uniform_buffer_descriptor_set{};
		uniform_buffer_descriptor_set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		uniform_buffer_descriptor_set.dstSet = global_descriptor_set;
		uniform_buffer_descriptor_set.dstBinding = 0; // also set to 3 in the shader
		uniform_buffer_descriptor_set.dstArrayElement = 0; // offset
		uniform_buffer_descriptor_set.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		uniform_buffer_descriptor_set.descriptorCount = 1;
		uniform_buffer_descriptor_set.pBufferInfo = &buffer_info;
		uniform_buffer_descriptor_set.pImageInfo = nullptr;
		uniform_buffer_descriptor_set.pTexelBufferView = nullptr;

		vkUpdateDescriptorSets(get_logical_device(), 
								1, 
								&uniform_buffer_descriptor_set, 
								0, 
								nullptr);
	}

	// high frequency descriptor sets allocations, i.e. per object descriptor sets
	{
		const int num_sets = get_max_descriptor_sets() - MAX_IMGUI_DESCRIPTOR_SETS - MAX_LOW_FREQ_DESCRIPTOR_SETS;
		std::vector<VkDescriptorSetLayout> layouts(num_sets, high_freq_descriptor_set_layout);
		VkDescriptorSetAllocateInfo alloc_info{};
		alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		alloc_info.descriptorPool = descriptor_pool;
		alloc_info.descriptorSetCount = num_sets;
		alloc_info.pSetLayouts = layouts.data();
		std::vector<VkDescriptorSet> descriptor_sets(num_sets);
		if (vkAllocateDescriptorSets(get_logical_device(), &alloc_info, descriptor_sets.data()) != VK_SUCCESS)
			throw std::runtime_error("GraphicsEnginePool: failed to allocate descriptor sets!");
		for (auto& descriptor_set : descriptor_sets)
			available_descriptor_sets.push(descriptor_set);
	}
}

int GraphicsEnginePool::get_max_descriptor_sets() const
{
	const int MAX_PER_SWAPCHAIN_IMAGE_DESCRIPTOR_SETS = 1000;
	return GraphicsEngineSwapChain::EXPECTED_NUM_SWAPCHAIN_IMAGES * MAX_PER_SWAPCHAIN_IMAGE_DESCRIPTOR_SETS;
	// return MAX_PER_SWAPCHAIN_IMAGE_DESCRIPTOR_SETS;
}

std::vector<VkDescriptorSet> GraphicsEnginePool::reserve_descriptor_sets(int n)
{
	// the reason this function is called 3x for every object spawn
	// is because we have a bit of a design problem
	// we really should be having per frame per object resources
	// i.e. for every object in every frame in a swapchain, it should have its own descriptor sets
	fmt::print("GraphicsEnginePool::reserve_descriptor_sets: available_sets:={}, requested_sets:={}\n", available_descriptor_sets.size(), n);

	if (n > available_descriptor_sets.size())
		throw std::runtime_error("GraphicsEnginePool: not enough available descriptor sets!");

	std::vector<VkDescriptorSet> sets(n);
	for (int i = 0; i < n; i++)
	{
		sets[i] = available_descriptor_sets.front();
		available_descriptor_sets.pop();
	}

	return sets;
}

void GraphicsEnginePool::free_descriptor_sets(std::vector<VkDescriptorSet>& sets)
{
	fmt::print("GraphicsEnginePool::free_descriptor_sets: available_sets:={}, amount_to_free:={}\n", available_descriptor_sets.size(), sets.size());
	for (auto& set : sets)
		available_descriptor_sets.push(set);
}