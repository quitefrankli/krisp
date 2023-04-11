#pragma once

#include "graphics_resource_manager.hpp"

#include "graphics_engine.hpp"
#include "queues.hpp"
#include "uniform_buffer_object.hpp"

#include <fmt/core.h>


template<typename GraphicsEngineT>
GraphicsResourceManager<GraphicsEngineT>::GraphicsResourceManager(GraphicsEngineT& engine) :
	GraphicsEngineBaseModule<GraphicsEngineT>(engine)
{
	static_assert(
		MAX_LOW_FREQ_DESCRIPTOR_SETS + MAX_HIGH_FREQ_DESCRIPTOR_SETS + 
		MAX_RAY_TRACING_DESCRIPTOR_SETS + MAX_IMGUI_DESCRIPTOR_SETS <= get_max_descriptor_sets(), 
		"GraphicsResourceManager: too many descriptor sets!");
	create_command_pool();
	create_descriptor_set_layouts();
	create_descriptor_pool();

	// global uniform buffer
	create_buffer(sizeof(GlobalUniformBufferObject),
				  VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
				  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				  global_uniform_buffer,
				  global_uniform_buffer_memory);

	allocate_rasterization_dsets();
	allocate_raytracing_dsets();
	// graphics engine only needs 1 global low freq desc set, hence only this is updated here
	initialise_global_descriptor_set();
}

template<typename GraphicsEngineT>
GraphicsResourceManager<GraphicsEngineT>::~GraphicsResourceManager()
{
	vkDestroyCommandPool(get_logical_device(), command_pool, nullptr);
	vkDestroyDescriptorPool(get_logical_device(), descriptor_pool, nullptr);

	for (auto layout : get_all_descriptor_set_layouts())
	{
		vkDestroyDescriptorSetLayout(get_logical_device(), layout, nullptr);
	}

	vkDestroyBuffer(get_logical_device(), global_uniform_buffer, nullptr);
	vkFreeMemory(get_logical_device(), global_uniform_buffer_memory, nullptr);
}

template<typename GraphicsEngineT>
void GraphicsResourceManager<GraphicsEngineT>::create_command_pool()
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

template<typename GraphicsEngineT>
void GraphicsResourceManager<GraphicsEngineT>::create_descriptor_pool()
{
	const auto& engine = get_graphics_engine();

	VkDescriptorPoolSize uniform_buffer_pool_size{};
	uniform_buffer_pool_size.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	// max number of uniform buffers per descriptor set
	uniform_buffer_pool_size.descriptorCount = MAX_UNIFORMS_PER_DESCRIPTOR_SET * get_max_descriptor_sets();

	VkDescriptorPoolSize combined_image_sampler_pool_size{};
	combined_image_sampler_pool_size.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	// max number of combined image samplers per descriptor set
	combined_image_sampler_pool_size.descriptorCount = MAX_COMBINED_IMAGE_SAMPLERS_PER_DESCRIPTOR_SET * get_max_descriptor_sets();

	// for ray tracing
	VkDescriptorPoolSize tlas_pool_size{};
	tlas_pool_size.type = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
	tlas_pool_size.descriptorCount = get_max_descriptor_sets(); // TODO: this needs a proper value

	VkDescriptorPoolSize rt_storage_image_pool_size{};
	rt_storage_image_pool_size.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	rt_storage_image_pool_size.descriptorCount = get_max_descriptor_sets();

	std::vector<VkDescriptorPoolSize> pool_sizes {
		uniform_buffer_pool_size, 
		combined_image_sampler_pool_size,
		tlas_pool_size,
		rt_storage_image_pool_size
	};

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

	fmt::print("GraphicsResourceManager::create_descriptor_pool: max_sets:={}\n"
		"\tper_set_max_uniform_buffer_count:={}\n"
		"\tper_set_max_combined_image_sampler_count:={}\n",
		poolInfo.maxSets, uniform_buffer_pool_size.descriptorCount, combined_image_sampler_pool_size.descriptorCount);

	if (vkCreateDescriptorPool(get_logical_device(), &poolInfo, nullptr, &descriptor_pool) != VK_SUCCESS)
	{
		throw std::runtime_error("GraphicsEngine::create_descriptor_pool: failed to create descriptor pool!");
	}
}

template<typename GraphicsEngineT>
void GraphicsResourceManager<GraphicsEngineT>::create_descriptor_set_layouts()
{
	auto create_layout = [this](const std::vector<VkDescriptorSetLayoutBinding>& bindings, VkDescriptorSetLayout* layout)
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

		const std::vector<VkDescriptorSetLayoutBinding> bindings{ ubo_layout_binding, sampler_layout_binding };
		create_layout(bindings, &high_freq_descriptor_set_layout);
	}

	// global uniforms i.e. camera & lighting
	{
		VkDescriptorSetLayoutBinding gubo_layout_binding{};
		gubo_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		gubo_layout_binding.binding = 0; // this must be synced with the one in the shaders
		gubo_layout_binding.descriptorCount = 1;
		// defines which shader stage the descriptor is going to be referenced
		gubo_layout_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR;
		gubo_layout_binding.pImmutableSamplers = nullptr; // only relevant for image sampling related descriptors

		const std::vector<VkDescriptorSetLayoutBinding> bindings{ gubo_layout_binding };
		create_layout(bindings, &low_freq_descriptor_set_layout);
	}
	
	// ray tracing
	{
		ray_tracing_resources.tlas_binding = {};
		ray_tracing_resources.tlas_binding.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
		ray_tracing_resources.tlas_binding.binding = 0; // this must be synced with the one in the shaders
		ray_tracing_resources.tlas_binding.descriptorCount = 1;
		ray_tracing_resources.tlas_binding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

		ray_tracing_resources.output_binding = {};
		ray_tracing_resources.output_binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		ray_tracing_resources.output_binding.binding = 1; // this must be synced with the one in the shaders
		ray_tracing_resources.output_binding.descriptorCount = 1;
		ray_tracing_resources.output_binding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

		const std::vector<VkDescriptorSetLayoutBinding> bindings = ray_tracing_resources.get_layout_bindings();
		create_layout(bindings, &ray_tracing_resources.descriptor_set_layout);
	}
}

template<typename GraphicsEngineT>
void GraphicsResourceManager<GraphicsEngineT>::allocate_rasterization_dsets()
{
	// allocation + writing for low frequency descriptor sets
	{
		VkDescriptorSetAllocateInfo alloc_info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
		alloc_info.descriptorPool = descriptor_pool;
		alloc_info.descriptorSetCount = MAX_LOW_FREQ_DESCRIPTOR_SETS;
		alloc_info.pSetLayouts = &low_freq_descriptor_set_layout;

		if (vkAllocateDescriptorSets(
			get_logical_device(), 
			&alloc_info, 
			&global_descriptor_set) != VK_SUCCESS)
		{
			throw std::runtime_error("GraphicsResourceManager: failed to allocate low freq descriptor sets!");
		}
	}

	// high frequency descriptor sets allocations, i.e. per object descriptor sets
	{
		std::vector<VkDescriptorSetLayout> layouts(MAX_HIGH_FREQ_DESCRIPTOR_SETS, high_freq_descriptor_set_layout);
		VkDescriptorSetAllocateInfo alloc_info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
		alloc_info.descriptorPool = descriptor_pool;
		alloc_info.descriptorSetCount = MAX_HIGH_FREQ_DESCRIPTOR_SETS;
		alloc_info.pSetLayouts = layouts.data();
		std::vector<VkDescriptorSet> descriptor_sets(MAX_HIGH_FREQ_DESCRIPTOR_SETS);
		if (vkAllocateDescriptorSets(get_logical_device(), &alloc_info, descriptor_sets.data()) != VK_SUCCESS)
		{
			throw std::runtime_error("GraphicsResourceManager: failed to allocate high freq descriptor sets!");
		}
		for (auto& dset : descriptor_sets)
		{
			available_high_freq_dsets.push(dset);
		}
	}
}

template<typename GraphicsEngineT>
void GraphicsResourceManager<GraphicsEngineT>::allocate_raytracing_dsets()
{
	std::vector<VkDescriptorSetLayout> layouts(MAX_RAY_TRACING_DESCRIPTOR_SETS, ray_tracing_resources.descriptor_set_layout);
	VkDescriptorSetAllocateInfo alloc_info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
	alloc_info.descriptorPool = descriptor_pool;
	alloc_info.descriptorSetCount = MAX_RAY_TRACING_DESCRIPTOR_SETS;
	alloc_info.pSetLayouts = layouts.data();

	std::vector<VkDescriptorSet> descriptor_sets(MAX_RAY_TRACING_DESCRIPTOR_SETS);
	if (vkAllocateDescriptorSets(get_logical_device(), &alloc_info, descriptor_sets.data()) != VK_SUCCESS)
	{
		throw std::runtime_error("GraphicsResourceManager: failed to allocate ray tracing descriptor set!");
	}
	for (auto& dset : descriptor_sets)
	{
		available_raytracing_dsets.push(dset);
	}
}

template<typename GraphicsEngineT>
std::vector<VkDescriptorSet> GraphicsResourceManager<GraphicsEngineT>::reserve_high_frequency_dsets(uint32_t n)
{
	// the reason this function is called 3x for every object spawn
	// is because we have a bit of a design problem
	// we really should be having per frame per object resources
	// i.e. for every object in every frame in a swapchain, it should have its own descriptor sets
	// fmt::print("GraphicsResourceManager::reserve_high_frequency_dsets: available_sets:={}, requested_sets:={}\n", available_high_freq_dsets.size(), n);

	if (n > available_high_freq_dsets.size())
	{
		throw std::runtime_error("GraphicsResourceManager: not enough available descriptor sets!");
	}

	std::vector<VkDescriptorSet> sets;
	sets.reserve(n);
	for (int i = 0; i < n; i++)
	{
		sets.push_back(available_high_freq_dsets.front());
		available_high_freq_dsets.pop();
	}

	return sets;
}

template<typename GraphicsEngineT>
std::vector<VkDescriptorSet> GraphicsResourceManager<GraphicsEngineT>::reserve_raytracing_dsets(uint32_t n)
{
	if (n > available_raytracing_dsets.size())
	{
		throw std::runtime_error("GraphicsResourceManager: not enough available ray tracing descriptor sets!");
	}

	std::vector<VkDescriptorSet> sets;
	sets.reserve(n);
	for (int i = 0; i < n; i++)
	{
		sets.push_back(available_raytracing_dsets.front());
		available_raytracing_dsets.pop();
	}

	return sets;
}

template<typename GraphicsEngineT>
void GraphicsResourceManager<GraphicsEngineT>::initialise_global_descriptor_set()
{
	VkDescriptorBufferInfo buffer_info{};
	buffer_info.buffer = global_uniform_buffer;
	buffer_info.offset = 0;
	buffer_info.range = sizeof(GlobalUniformBufferObject);

	VkWriteDescriptorSet dset_write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
	dset_write.dstSet = global_descriptor_set;
	dset_write.dstBinding = 0; // also set to 3 in the shader
	dset_write.dstArrayElement = 0; // offset
	dset_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	dset_write.descriptorCount = 1;
	dset_write.pBufferInfo = &buffer_info;
	dset_write.pImageInfo = nullptr;
	dset_write.pTexelBufferView = nullptr;

	vkUpdateDescriptorSets(
		get_logical_device(), 
		1, 
		&dset_write, 
		0, 
		nullptr);
}

template<typename GraphicsEngineT>
VkCommandBuffer GraphicsResourceManager<GraphicsEngineT>::create_command_buffer()
{
	VkCommandBufferAllocateInfo allocation_info{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
	allocation_info.commandPool = command_pool;
	// specifies if allocated command buffers are primary or secondary command buffers, secondary can reuse primary
	allocation_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocation_info.commandBufferCount = 1;

	VkCommandBuffer command_buffer;
	if (vkAllocateCommandBuffers(get_logical_device(), &allocation_info, &command_buffer) != VK_SUCCESS)
	{
		throw std::runtime_error("GraphicsResourceManager: failed to allocate command buffers!");
	}

	return command_buffer;
}

template<typename GraphicsEngineT>
constexpr int GraphicsResourceManager<GraphicsEngineT>::get_max_descriptor_sets()
{
	constexpr int MAX_PER_SWAPCHAIN_IMAGE_DESCRIPTOR_SETS = 1000;
	return GraphicsEngineSwapChain<GraphicsEngineT>::EXPECTED_NUM_SWAPCHAIN_IMAGES * MAX_PER_SWAPCHAIN_IMAGE_DESCRIPTOR_SETS;
	// return MAX_PER_SWAPCHAIN_IMAGE_DESCRIPTOR_SETS;
}

template<typename GraphicsEngineT>
std::vector<VkDescriptorSetLayout> GraphicsResourceManager<GraphicsEngineT>::get_rasterization_descriptor_set_layouts() const
{
	return { high_freq_descriptor_set_layout, low_freq_descriptor_set_layout };
}

template<typename GraphicsEngineT>
std::vector<VkDescriptorSetLayout> GraphicsResourceManager<GraphicsEngineT>::get_raytracing_descriptor_set_layouts() const
{
	return { ray_tracing_resources.descriptor_set_layout, low_freq_descriptor_set_layout };
}

template<typename GraphicsEngineT>
std::vector<VkDescriptorSetLayout> GraphicsResourceManager<GraphicsEngineT>::get_all_descriptor_set_layouts()
{
	std::unordered_set<VkDescriptorSetLayout> layouts;
	for (auto layout : get_rasterization_descriptor_set_layouts())
	{
		if (layout != VK_NULL_HANDLE)
		{
			layouts.insert(layout);
		}
	}
	for (auto layout : get_raytracing_descriptor_set_layouts())
	{
		if (layout != VK_NULL_HANDLE)
		{
			layouts.insert(layout);
		}
	}

	return std::vector<VkDescriptorSetLayout>(layouts.begin(), layouts.end());
}

template<typename GraphicsEngineT>
void GraphicsResourceManager<GraphicsEngineT>::free_high_frequency_dsets(std::vector<VkDescriptorSet>& sets)
{
	// fmt::print("GraphicsResourceManager::free_high_frequency_dsets: available_sets:={}, amount_to_free:={}\n", available_high_freq_dsets.size(), sets.size());
	for (auto& set : sets)
	{
		available_high_freq_dsets.push(set);
	}
}

template<typename GraphicsEngineT>
void GraphicsResourceManager<GraphicsEngineT>::free_raytracing_dsets(std::vector<VkDescriptorSet>& sets)
{
	// fmt::print("GraphicsResourceManager::free_raytracing_dsets: available_sets:={}, amount_to_free:={}\n", available_high_freq_dsets.size(), sets.size());
	for (auto& set : sets)
	{
		available_raytracing_dsets.push(set);
	}
}

