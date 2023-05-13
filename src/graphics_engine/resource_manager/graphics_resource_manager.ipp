#pragma once

#include "graphics_resource_manager.hpp"

#include "graphics_engine/graphics_engine.hpp"
#include "graphics_engine/queues.hpp"
#include "shared_data_structures.hpp"

#include <fmt/core.h>


template<typename GraphicsEngineT>
GraphicsResourceManager<GraphicsEngineT>::GraphicsResourceManager(GraphicsEngineT& engine) :
	GraphicsBufferManager(engine)
{
	create_command_pool();
	create_descriptor_set_layouts();
	create_descriptor_pool();

	allocate_rasterization_dsets();
	allocate_raytracing_dsets();
	allocate_mesh_data_dsets();
	// graphics engine only needs 1 global low freq desc set, hence only this is updated here
	initialise_global_descriptor_set();

	// same with mesh data i.e. vertices and indices
	initialise_mesh_data_descriptor_set();
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
	uniform_buffer_pool_size.descriptorCount = MAX_UNIFORMS_PER_DESCRIPTOR_SET * MAX_HIGH_FREQ_DESCRIPTOR_SETS;

	VkDescriptorPoolSize combined_image_sampler_pool_size{};
	combined_image_sampler_pool_size.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	// max number of combined image samplers per descriptor set
	combined_image_sampler_pool_size.descriptorCount = MAX_COMBINED_IMAGE_SAMPLERS_PER_DESCRIPTOR_SET * MAX_HIGH_FREQ_DESCRIPTOR_SETS;

	// for ray tracing
	VkDescriptorPoolSize tlas_pool_size{};
	tlas_pool_size.type = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
	tlas_pool_size.descriptorCount = MAX_RAY_TRACING_DESCRIPTOR_SETS;

	VkDescriptorPoolSize rt_storage_image_pool_size{};
	rt_storage_image_pool_size.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	rt_storage_image_pool_size.descriptorCount = MAX_RAY_TRACING_DESCRIPTOR_SETS;

	// for meshes, materials and bones
	VkDescriptorPoolSize storage_buffer_pool_size{};
	storage_buffer_pool_size.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	storage_buffer_pool_size.descriptorCount = MAX_STORAGE_BUFFER_DESCRIPTOR_SETS;

	std::vector<VkDescriptorPoolSize> pool_sizes {
		uniform_buffer_pool_size, 
		combined_image_sampler_pool_size,
		tlas_pool_size,
		rt_storage_image_pool_size,
		storage_buffer_pool_size
	};

	VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
	poolInfo.poolSizeCount = pool_sizes.size();
	poolInfo.pPoolSizes = pool_sizes.data();
	// defines maximum number of descriptor sets that may be allocated
	poolInfo.maxSets = MAX_DESCRIPTOR_SETS;

	LOG_INFO(Utility::get().get_logger(), 
			 "GraphicsResourceManager::create_descriptor_pool: max_sets:={}\n",
		   	 poolInfo.maxSets);

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

	// Object
	{
		VkDescriptorSetLayoutBinding ubo_layout_binding{};
		ubo_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		ubo_layout_binding.binding = SDS::RASTERIZATION_OBJECT_DATA_BINDING;
		ubo_layout_binding.descriptorCount = 1;
		ubo_layout_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT; // defines which shader stage the descriptor is going to be referenced
		ubo_layout_binding.pImmutableSamplers = nullptr; // only relevant for image sampling related descriptors

		// buffer of bone data containing transformation matrices
		VkDescriptorSetLayoutBinding bone_layout_binding{};
		bone_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		bone_layout_binding.binding = 3;
		bone_layout_binding.descriptorCount = 1;
		bone_layout_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		bone_layout_binding.pImmutableSamplers = nullptr;

		const std::vector<VkDescriptorSetLayoutBinding> bindings{ ubo_layout_binding, bone_layout_binding };
		create_layout(bindings, &rasterization_high_freq_per_obj_dset_layout);
	}

	// Materials
	{

		VkDescriptorSetLayoutBinding sampler_layout_binding{};
		sampler_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		sampler_layout_binding.binding = SDS::RASTERIZATION_ALBEDO_TEXTURE_DATA_BINDING;
		sampler_layout_binding.descriptorCount = 1;
		sampler_layout_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT; // defines which shader stage the descriptor is going to be referenced
		sampler_layout_binding.pImmutableSamplers = nullptr; // only relevant for image sampling related descriptors

		VkDescriptorSetLayoutBinding materials_layout_binding{};
		materials_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		materials_layout_binding.binding = SDS::RASTERIZATION_MATERIAL_DATA_BINDING;
		materials_layout_binding.descriptorCount = 1;
		materials_layout_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		materials_layout_binding.pImmutableSamplers = nullptr;

		const std::vector<VkDescriptorSetLayoutBinding> bindings{ sampler_layout_binding, materials_layout_binding };
		create_layout(bindings, &rasterization_high_freq_per_shape_dset_layout);
	}

	// global uniforms i.e. camera & lighting
	{
		VkDescriptorSetLayoutBinding gubo_layout_binding{};
		gubo_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		gubo_layout_binding.binding = SDS::GLOBAL_DATA_BINDING;
		gubo_layout_binding.descriptorCount = 1;
		// defines which shader stage the descriptor is going to be referenced
		gubo_layout_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | 
			VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
		gubo_layout_binding.pImmutableSamplers = nullptr; // only relevant for image sampling related descriptors

		const std::vector<VkDescriptorSetLayoutBinding> bindings{ gubo_layout_binding };
		create_layout(bindings, &low_freq_descriptor_set_layout);
	}
	
	// ray tracing
	{
		ray_tracing_resources.tlas_binding = {};
		ray_tracing_resources.tlas_binding.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
		ray_tracing_resources.tlas_binding.binding = SDS::RAYTRACING_TLAS_DATA_BINDING;
		ray_tracing_resources.tlas_binding.descriptorCount = 1;
		ray_tracing_resources.tlas_binding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

		ray_tracing_resources.output_binding = {};
		ray_tracing_resources.output_binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		ray_tracing_resources.output_binding.binding = SDS::RAYTRACING_OUTPUT_IMAGE_BINDING;
		ray_tracing_resources.output_binding.descriptorCount = 1;
		ray_tracing_resources.output_binding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

		const std::vector<VkDescriptorSetLayoutBinding> bindings = ray_tracing_resources.get_layout_bindings();
		create_layout(bindings, &ray_tracing_resources.descriptor_set_layout);
	}

	// mesh data: vertices, indices
	{
		VkDescriptorSetLayoutBinding buffer_mapper_binding{};
		buffer_mapper_binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		buffer_mapper_binding.binding = SDS::RAYTRACING_BUFFER_MAPPER_BINDING;
		buffer_mapper_binding.descriptorCount = 1;
		buffer_mapper_binding.stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
		buffer_mapper_binding.pImmutableSamplers = nullptr;

		VkDescriptorSetLayoutBinding vertices_binding{};
		vertices_binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		vertices_binding.binding = SDS::RAYTRACING_VERTICES_DATA_BINDING;
		vertices_binding.descriptorCount = 1;
		vertices_binding.stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
		vertices_binding.pImmutableSamplers = nullptr;

		VkDescriptorSetLayoutBinding indices_binding{};
		indices_binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		indices_binding.binding = SDS::RAYTRACING_INDICES_DATA_BINDING;
		indices_binding.descriptorCount = 1;
		indices_binding.stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
		indices_binding.pImmutableSamplers = nullptr;		

		const std::vector<VkDescriptorSetLayoutBinding> bindings{ buffer_mapper_binding, vertices_binding, indices_binding };
		create_layout(bindings, &mesh_data_descriptor_set_layout);
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

	// high frequency descriptor sets allocations, i.e. per object and per shape dsets
	// this might cause slow startup, we may be able to optimise by allocating blocks at runtime instead
	const auto allocate_high_freq_dsets = [&](VkDescriptorSetLayout layout, std::queue<VkDescriptorSet>& q)
	{
		std::vector<VkDescriptorSetLayout> layouts(MAX_HIGH_FREQ_DESCRIPTOR_SETS, layout);
		VkDescriptorSetAllocateInfo alloc_info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
		alloc_info.descriptorPool = descriptor_pool;
		alloc_info.descriptorSetCount = layouts.size();
		alloc_info.pSetLayouts = layouts.data();
		std::vector<VkDescriptorSet> descriptor_sets(MAX_HIGH_FREQ_DESCRIPTOR_SETS);
		if (vkAllocateDescriptorSets(get_logical_device(), &alloc_info, descriptor_sets.data()) != VK_SUCCESS)
		{
			throw std::runtime_error("GraphicsResourceManager: failed to allocate high freq descriptor sets!");
		}
		for (auto dset : descriptor_sets)
		{
			q.push(dset);
		}
	};

	allocate_high_freq_dsets(rasterization_high_freq_per_obj_dset_layout, available_high_freq_per_obj_dsets);
	allocate_high_freq_dsets(rasterization_high_freq_per_shape_dset_layout, available_high_freq_per_shape_dsets);
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
void GraphicsResourceManager<GraphicsEngineT>::allocate_mesh_data_dsets()
{
	std::vector<VkDescriptorSetLayout> layouts(MAX_MESH_DATA_DESCRIPTOR_SETS, mesh_data_descriptor_set_layout);
	VkDescriptorSetAllocateInfo alloc_info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
	alloc_info.descriptorPool = descriptor_pool;
	alloc_info.descriptorSetCount = MAX_MESH_DATA_DESCRIPTOR_SETS;
	alloc_info.pSetLayouts = layouts.data();
	if (vkAllocateDescriptorSets(get_logical_device(), &alloc_info, &mesh_data_descriptor_set) != VK_SUCCESS)
	{
		throw std::runtime_error("GraphicsResourceManager: failed to allocate mesh data descriptor set!");
	}
}

template<typename GraphicsEngineT>
void GraphicsResourceManager<GraphicsEngineT>::initialise_global_descriptor_set()
{
	VkDescriptorBufferInfo buffer_info{};
	buffer_info.buffer = get_global_uniform_buffer();
	buffer_info.offset = 0;
	buffer_info.range = sizeof(SDS::GlobalData);

	VkWriteDescriptorSet dset_write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
	dset_write.dstSet = global_descriptor_set;
	dset_write.dstBinding = SDS::GLOBAL_DATA_BINDING;
	dset_write.dstArrayElement = 0;
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
void GraphicsResourceManager<GraphicsEngineT>::initialise_mesh_data_descriptor_set()
{
	// create descriptor set for object's mesh and material (not implemented yet) data
	VkDescriptorBufferInfo buffer_mapper_info{};
	buffer_mapper_info.buffer = get_mapping_buffer();
	buffer_mapper_info.offset = 0;
	buffer_mapper_info.range = MAPPING_BUFFER_CAPACITY;
	VkWriteDescriptorSet buffer_mapper_dset_write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
	buffer_mapper_dset_write.dstSet = mesh_data_descriptor_set;
	buffer_mapper_dset_write.dstBinding = SDS::BUFFER_MAPPER_BINDING;
	buffer_mapper_dset_write.dstArrayElement = 0;
	buffer_mapper_dset_write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	buffer_mapper_dset_write.descriptorCount = 1;
	buffer_mapper_dset_write.pBufferInfo = &buffer_mapper_info;

	VkDescriptorBufferInfo vertices_buffer_info{};
	vertices_buffer_info.buffer = get_vertex_buffer();
	vertices_buffer_info.offset = 0;
	vertices_buffer_info.range = VERTEX_BUFFER_CAPACITY;
	VkWriteDescriptorSet vertices_dset_write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
	vertices_dset_write.dstSet = mesh_data_descriptor_set;
	vertices_dset_write.dstBinding = SDS::VERTICES_DATA_BINDING;
	vertices_dset_write.dstArrayElement = 0;
	vertices_dset_write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	vertices_dset_write.descriptorCount = 1;
	vertices_dset_write.pBufferInfo = &vertices_buffer_info;

	VkDescriptorBufferInfo indices_buffer_info{};
	indices_buffer_info.buffer = get_index_buffer();
	indices_buffer_info.offset = 0;
	indices_buffer_info.range = INDEX_BUFFER_CAPACITY;
	VkWriteDescriptorSet indices_dset_write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
	indices_dset_write.dstSet = mesh_data_descriptor_set;
	indices_dset_write.dstBinding = SDS::INDICES_DATA_BINDING;
	indices_dset_write.dstArrayElement = 0;
	indices_dset_write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	indices_dset_write.descriptorCount = 1;
	indices_dset_write.pBufferInfo = &indices_buffer_info;

	std::vector<VkWriteDescriptorSet> dsets{buffer_mapper_dset_write, vertices_dset_write, indices_dset_write};

	vkUpdateDescriptorSets(
		get_logical_device(),
		static_cast<uint32_t>(dsets.size()),
		dsets.data(),
		0,
		nullptr);
}

template<typename GraphicsEngineT>
std::vector<VkDescriptorSet> GraphicsResourceManager<GraphicsEngineT>::reserve_dsets_common_impl(
	std::queue<VkDescriptorSet>& queue,
	uint32_t n)
{
	if (n > queue.size())
	{
		throw std::runtime_error("GraphicsResourceManager: not enough available descriptor sets!");
	}

	std::vector<VkDescriptorSet> sets;
	sets.reserve(n);
	for (int i = 0; i < n; i++)
	{
		sets.push_back(queue.front());
		queue.pop();
	}

	return sets;;
}

template<typename GraphicsEngineT>
void GraphicsResourceManager<GraphicsEngineT>::free_dsets_common_impl(
	std::queue<VkDescriptorSet>& queue,
	std::vector<VkDescriptorSet>& dsets)
{
	for (auto dset : dsets)
	{
		queue.push(dset);
	}
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
std::vector<VkDescriptorSetLayout> GraphicsResourceManager<GraphicsEngineT>::
	get_rasterization_descriptor_set_layouts() const
{
	return { 
		low_freq_descriptor_set_layout,
		rasterization_high_freq_per_obj_dset_layout,
		rasterization_high_freq_per_shape_dset_layout 
	};
}

template<typename GraphicsEngineT>
std::vector<VkDescriptorSetLayout> GraphicsResourceManager<GraphicsEngineT>::
	get_raytracing_descriptor_set_layouts() const
{
	return { 
		low_freq_descriptor_set_layout,
		ray_tracing_resources.descriptor_set_layout, 
		mesh_data_descriptor_set_layout
	};
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