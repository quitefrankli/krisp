#include "graphics_buffer_manager.hpp"

#include <numeric>


template<typename GraphicsEngineT>
GraphicsBufferManager<GraphicsEngineT>::GraphicsBufferManager(GraphicsEngineT& engine) :
	GraphicsEngineBaseModule<GraphicsEngineT>(engine),
	vertex_buffer(create_buffer(VERTEX_BUFFER_CAPACITY, VERTEX_BUFFER_USAGE_FLAGS, VERTEX_BUFFER_MEMORY_FLAGS, 4)),
	index_buffer(create_buffer(INDEX_BUFFER_CAPACITY, INDEX_BUFFER_USAGE_FLAGS, INDEX_BUFFER_MEMORY_FLAGS, 4)),
	uniform_buffer(create_buffer(
		UNIFORM_BUFFER_CAPACITY, 
		UNIFORM_BUFFER_USAGE_FLAGS, 
		UNIFORM_BUFFER_MEMORY_FLAGS, 
		engine.get_device_module().get_physical_device_properties().properties.limits.minUniformBufferOffsetAlignment)),
	materials_buffer(create_buffer(
		MATERIALS_BUFFER_CAPACITY, 
		MATERIALS_BUFFER_USAGE_FLAGS, 
		MATERIALS_BUFFER_MEMORY_FLAGS, 
		engine.get_device_module().get_physical_device_properties().properties.limits.minStorageBufferOffsetAlignment)),
	global_uniform_buffer(create_buffer(
		GLOBAL_UNIFORM_BUFFER_CAPACITY, GLOBAL_UNIFORM_BUFFER_USAGE_FLAGS, GLOBAL_UNIFORM_BUFFER_MEMORY_FLAGS)),
	mapping_buffer(create_buffer(
		MAPPING_BUFFER_CAPACITY, MAPPING_BUFFER_USAGE_FLAGS, MAPPING_BUFFER_MEMORY_FLAGS), sizeof(BufferMapEntry))
{
	// reserve the first slot in the global uniform buffer for gubo (we only ever use 1 slot)
	global_uniform_buffer.reserve_slot(0, global_uniform_buffer.get_capacity());
}

template<typename GraphicsEngineT>
GraphicsBufferManager<GraphicsEngineT>::~GraphicsBufferManager() 
{
	vertex_buffer.destroy(get_logical_device());
	index_buffer.destroy(get_logical_device());
	uniform_buffer.destroy(get_logical_device());
	materials_buffer.destroy(get_logical_device());
	global_uniform_buffer.destroy(get_logical_device());
	mapping_buffer.destroy(get_logical_device());
}

template<typename GraphicsEngineT>
void GraphicsBufferManager<GraphicsEngineT>::write_to_uniform_buffer(
	const SDS::ObjectData& ubos,
	uint32_t id)
{
	std::byte* mapped_memory = uniform_buffer.map_slot(id, get_logical_device());
	*reinterpret_cast<SDS::ObjectData*>(mapped_memory) = ubos;
	uniform_buffer.unmap_slot(get_logical_device());
}

template<typename GraphicsEngineT>
void GraphicsBufferManager<GraphicsEngineT>::write_to_global_uniform_buffer(const SDS::GlobalData& ubo)
{
	std::byte* mapped_memory = global_uniform_buffer.map_slot(0, get_logical_device());
	*reinterpret_cast<SDS::GlobalData*>(mapped_memory) = ubo;
	global_uniform_buffer.unmap_slot(get_logical_device());
}

template<typename GraphicsEngineT>
void GraphicsBufferManager<GraphicsEngineT>::write_shapes_to_buffers(const std::vector<Shape>& shapes, uint32_t id)
{
	// assume that the size of the data in shapes is equal to the size of the buffer slot
	const auto vertex_slot = vertex_buffer.get_slot(id);
	const auto index_slot = index_buffer.get_slot(id);
 
	stage_data_to_buffer(vertex_buffer.get_buffer(), vertex_slot.offset, vertex_slot.size,
	[&shapes](std::byte* destination)
	{
		for (auto& shape : shapes)
		{
			const uint32_t vertex_set_size = shape.vertices.size() * sizeof(typename decltype(shape.vertices)::value_type);
			std::memcpy(destination, shape.vertices.data(), vertex_set_size);
			destination += vertex_set_size;
		}
	});

	stage_data_to_buffer(index_buffer.get_buffer(), index_slot.offset, index_slot.size,
	[&shapes](std::byte* destination)
	{
		for (auto& shape : shapes)
		{
			const uint32_t index_set_size = shape.indices.size() * sizeof(typename decltype(shape.indices)::value_type);
			std::memcpy(destination, shape.indices.data(), index_set_size);
			destination += index_set_size;
		}
	});
}

template<typename GraphicsEngineT>
void GraphicsBufferManager<GraphicsEngineT>::write_to_materials_buffer(const SDS::MaterialData& material, ShapeID id)
{
	// TODO: lots of objects will share the same material, need to come up with way to hash materials and only write unique ones
	const auto slot = materials_buffer.get_slot(id.get_underlying());
	stage_data_to_buffer(materials_buffer.get_buffer(), slot.offset, slot.size,
	[&material](std::byte* destination)
	{
		std::memcpy(destination, &material, sizeof(material));
	});
}

template<typename GraphicsEngineT>
void GraphicsBufferManager<GraphicsEngineT>::write_to_mapping_buffer(const BufferMapEntry& entry, uint32_t id)
{
	stage_data_to_buffer(mapping_buffer.get_buffer(), mapping_buffer.get_slot_offset(id), sizeof(entry), 
	[&entry](std::byte* destination)
	{
		std::memcpy(destination, &entry, sizeof(entry));
	});
}

template<typename GraphicsEngineT>
GraphicsBuffer GraphicsBufferManager<GraphicsEngineT>::create_buffer(size_t size,
                                                                       VkBufferUsageFlags usage_flags,
                                                                       VkMemoryPropertyFlags memory_flags,
																	   uint32_t alignment)
{
	VkBuffer buffer;
	VkDeviceMemory memory;

	VkBufferCreateInfo buffer_create_info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
	buffer_create_info.size = size;
	buffer_create_info.usage = usage_flags;
	buffer_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE; // will only be used in graphics queue

	if (vkCreateBuffer(get_logical_device(), &buffer_create_info, nullptr, &buffer) != VK_SUCCESS)
	{
		throw std::runtime_error("GraphicsEngine::create_buffer: failed to create buffer!");
	}

	VkMemoryRequirements memory_requirements;
	vkGetBufferMemoryRequirements(get_logical_device(), buffer, &memory_requirements);
	
	VkMemoryAllocateInfo memory_allocate_info{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
	memory_allocate_info.allocationSize = memory_requirements.size;
	memory_allocate_info.memoryTypeIndex = 
		get_graphics_engine().find_memory_type(memory_requirements.memoryTypeBits, memory_flags);

	VkMemoryAllocateFlagsInfo memory_allocate_flags_info{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO};
	memory_allocate_flags_info.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;

	memory_allocate_info.pNext = &memory_allocate_flags_info;

	if (vkAllocateMemory(get_logical_device(), &memory_allocate_info, nullptr, &memory) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to allocate device buffer memory!");
	}

	vkBindBufferMemory(get_logical_device(), buffer, memory, 0);

	return GraphicsBuffer(buffer, memory, size, alignment);
}

template<typename GraphicsEngineT>
void GraphicsBufferManager<GraphicsEngineT>::create_buffer(size_t size,
														   VkBufferUsageFlags usage_flags,
														   VkMemoryPropertyFlags memory_flags,
														   VkBuffer& buffer,
														   VkDeviceMemory& buffer_memory)
{
	VkBufferCreateInfo buffer_create_info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
	buffer_create_info.size = size;
	buffer_create_info.usage = usage_flags;
	buffer_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE; // will only be used in graphics queue

	if (vkCreateBuffer(get_logical_device(), &buffer_create_info, nullptr, &buffer) != VK_SUCCESS)
	{
		throw std::runtime_error("GraphicsEngine::create_buffer: failed to create buffer!");
	}

	VkMemoryRequirements memory_requirements;
	vkGetBufferMemoryRequirements(get_logical_device(), buffer, &memory_requirements);
	
	VkMemoryAllocateInfo memory_allocate_info{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
	memory_allocate_info.allocationSize = memory_requirements.size;
	memory_allocate_info.memoryTypeIndex = 
		get_graphics_engine().find_memory_type(memory_requirements.memoryTypeBits, memory_flags);

	VkMemoryAllocateFlagsInfo memory_allocate_flags_info{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO};
	memory_allocate_flags_info.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;

	memory_allocate_info.pNext = &memory_allocate_flags_info;

	if (vkAllocateMemory(get_logical_device(), &memory_allocate_info, nullptr, &buffer_memory) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to allocate device buffer memory!");
	}

	vkBindBufferMemory(get_logical_device(), buffer, buffer_memory, 0);
}

template<typename GraphicsEngineT>
void GraphicsBufferManager<GraphicsEngineT>::stage_data_to_buffer(VkBuffer destination_buffer,
                                                                  const uint32_t destination_buffer_offset,
                                                                  const uint32_t size,
                                                                  const std::function<void(std::byte*)>& write_function)
{
	// create staging buffer, used for copying the data from staging to vertex buffer
	// the staging buffer is the buffer that can be accessed by CPU
	// TODO: this can be optimised in two ways
	// 1. something about using different queues https://www.reddit.com/r/vulkan/comments/pnweh0/vkcmdcopybuffer_performance_worse_on_nvidia_than/
	// 2. Use a large cached staging buffer so we don't need to recreate everytime
	GraphicsBuffer staging_buffer = create_buffer(
		size,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		// make sure that the memory heap is host coherent
		// this is because the driver may not immediately copy the data into the buffer memory
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	// fill the staging buffer
	void* mapped_data;
	vkMapMemory(get_logical_device(), staging_buffer.get_memory(), 0, size, 0, &mapped_data);
	write_function(reinterpret_cast<std::byte*>(mapped_data));
	vkUnmapMemory(get_logical_device(), staging_buffer.get_memory());

	// issue the command to copy from staging to device
	VkCommandBuffer command_buffer = get_graphics_engine().begin_single_time_commands();

	// actual copy command
	VkBufferCopy copy_region{};
	copy_region.srcOffset = 0;
	copy_region.dstOffset = destination_buffer_offset;
	copy_region.size = size;
	vkCmdCopyBuffer(command_buffer, staging_buffer.get_buffer(), destination_buffer, 1, &copy_region);

	get_graphics_engine().end_single_time_commands(command_buffer);

	// clean up staging buffer
	staging_buffer.destroy(get_logical_device());
}