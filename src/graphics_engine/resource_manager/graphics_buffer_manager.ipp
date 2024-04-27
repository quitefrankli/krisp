#include "graphics_buffer_manager.hpp"
#include "renderable/mesh.hpp"

#include <numeric>


GraphicsBufferManager::GraphicsBufferManager(GraphicsEngine& engine) :
	GraphicsEngineBaseModule(engine),
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
		GLOBAL_UNIFORM_BUFFER_CAPACITY, 
		GLOBAL_UNIFORM_BUFFER_USAGE_FLAGS, 
		GLOBAL_UNIFORM_BUFFER_MEMORY_FLAGS,
		engine.get_device_module().get_physical_device_properties().properties.limits.minUniformBufferOffsetAlignment)),
	mapping_buffer(create_buffer(
		MAPPING_BUFFER_CAPACITY, MAPPING_BUFFER_USAGE_FLAGS, MAPPING_BUFFER_MEMORY_FLAGS), sizeof(SDS::BufferMapEntry)),
	bone_buffer(create_buffer(
		BONE_BUFFER_CAPACITY, BONE_BUFFER_USAGE_FLAGS, BONE_BUFFER_MEMORY_FLAGS)),
	staging_buffer(create_buffer(INITIAL_STAGING_BUFFER_CAPACITY, STAGING_BUFFER_USAGE_FLAGS, STAGING_BUFFER_MEMORY_FLAGS))
{
	// reserve the first slot in the global uniform buffer for gubo (we only ever use 1 slot)
	for (uint32_t frame_idx = 0; frame_idx < GraphicsEngine::get_num_swapchain_images(); ++frame_idx)
	{
		global_uniform_buffer.reserve_slot(frame_idx, sizeof(SDS::GlobalData));
	}
}

GraphicsBufferManager::~GraphicsBufferManager() 
{
	vertex_buffer.destroy(get_logical_device());
	index_buffer.destroy(get_logical_device());
	uniform_buffer.destroy(get_logical_device());
	materials_buffer.destroy(get_logical_device());
	global_uniform_buffer.destroy(get_logical_device());
	mapping_buffer.destroy(get_logical_device());
	bone_buffer.destroy(get_logical_device());
	staging_buffer.destroy(get_logical_device());
}

void GraphicsBufferManager::write_to_uniform_buffer(EntityFrameID id, const SDS::ObjectData& ubos)
{
	std::byte* mapped_memory = uniform_buffer.map_slot(id.get_underlying(), get_logical_device());
	*reinterpret_cast<SDS::ObjectData*>(mapped_memory) = ubos;
	uniform_buffer.unmap_slot(get_logical_device());
}

void GraphicsBufferManager::write_to_global_uniform_buffer(uint32_t id, const SDS::GlobalData& ubo)
{
	std::byte* mapped_memory = global_uniform_buffer.map_slot(id, get_logical_device());
	*reinterpret_cast<SDS::GlobalData*>(mapped_memory) = ubo;
	global_uniform_buffer.unmap_slot(get_logical_device());
}

void GraphicsBufferManager::write_to_mapping_buffer(ObjectID id, const SDS::BufferMapEntry& entry)
{
	mapping_buffer.decrease_free_capacity(sizeof(entry));
	stage_data_to_buffer(mapping_buffer.get_buffer(), mapping_buffer.get_slot_offset(id.get_underlying()), sizeof(entry), 
	[&entry](std::byte* destination)
	{
		std::memcpy(destination, &entry, sizeof(entry));
	});
	update_buffer_stats();
}

void GraphicsBufferManager::write_to_bone_buffer(EntityFrameID id, const std::vector<SDS::Bone>& bones)
{
	assert(!bones.empty());
	std::byte* mapped_memory = bone_buffer.map_slot(id.get_underlying(), get_logical_device());
	std::memcpy(mapped_memory, bones.data(), bones.size() * sizeof(bones[0]));
	bone_buffer.unmap_slot(get_logical_device());
}

void GraphicsBufferManager::write_to_buffer(MaterialID id, const SDS::MaterialData& material)
{
	static std::unordered_set<MaterialID> cache;
	if (cache.emplace(id).second == false)
	{
		return;
	}

	reserve_buffer(id, sizeof(material));

	const auto slot = materials_buffer.get_slot(id.get_underlying());
	stage_data_to_buffer(materials_buffer.get_buffer(), slot.offset, slot.size,
	[&material](std::byte* destination)
	{
		std::memcpy(destination, &material, sizeof(material));
	});
}

void GraphicsBufferManager::write_to_buffer(MeshID id, const Mesh& mesh) 
{
	static std::unordered_set<MeshID> cache;
	if (cache.emplace(id).second == false)
	{
		return;
	}

	reserve_vertex_buffer(id, mesh.get_vertices_data_size());
	reserve_index_buffer(id, mesh.get_indices_data_size());

	const auto vertex_slot = vertex_buffer.get_slot(id.get_underlying());
	const auto index_slot = index_buffer.get_slot(id.get_underlying());

	stage_data_to_buffer(vertex_buffer.get_buffer(), vertex_slot.offset, vertex_slot.size,
	[&mesh](std::byte* destination)
	{
		std::memcpy(destination, mesh.get_vertices_data(), mesh.get_vertices_data_size());
	});

	stage_data_to_buffer(index_buffer.get_buffer(), index_slot.offset, index_slot.size,
	[&mesh](std::byte* destination)
	{
		std::memcpy(destination, mesh.get_indices_data(), mesh.get_indices_data_size());
	});
}

GraphicsBuffer GraphicsBufferManager::create_buffer(
	size_t size,
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

void GraphicsBufferManager::create_buffer_deprecated(size_t size,
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

void GraphicsBufferManager::stage_data_to_buffer(VkBuffer destination_buffer,
                                                                  const uint32_t destination_buffer_offset,
                                                                  const uint32_t size,
                                                                  const std::function<void(std::byte*)>& write_function)
{
	// TODO: this can be optimised:
	// 1. something about using different queues https://www.reddit.com/r/vulkan/comments/pnweh0/vkcmdcopybuffer_performance_worse_on_nvidia_than/
	if (staging_buffer.get_capacity() < size)
	{
		// recreate the staging buffer if it is too small
		staging_buffer.destroy(get_logical_device());
		new (&staging_buffer) GraphicsBuffer(create_buffer(size, STAGING_BUFFER_USAGE_FLAGS, STAGING_BUFFER_MEMORY_FLAGS));
	}

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
}

void GraphicsBufferManager::stage_data_to_image(
	VkImage destination_image,
	const uint32_t width,
	const uint32_t height,
	const size_t size,
	const std::function<void(std::byte*)>& write_function,
	const uint32_t layer_count)
{
	// TODO: this can be optimised:
	// 1. something about using different queues https://www.reddit.com/r/vulkan/comments/pnweh0/vkcmdcopybuffer_performance_worse_on_nvidia_than/
	if (staging_buffer.get_capacity() < size)
	{
		// recreate the staging buffer if it is too small
		staging_buffer.destroy(get_logical_device());
		new (&staging_buffer) GraphicsBuffer(create_buffer(size, STAGING_BUFFER_USAGE_FLAGS, STAGING_BUFFER_MEMORY_FLAGS));
	}

	// fill the staging buffer
	void* mapped_data;
	vkMapMemory(get_logical_device(), staging_buffer.get_memory(), 0, size, 0, &mapped_data);
	write_function(reinterpret_cast<std::byte*>(mapped_data));
	vkUnmapMemory(get_logical_device(), staging_buffer.get_memory());

	// issue the command to copy from staging to device
	VkCommandBuffer command_buffer = get_graphics_engine().begin_single_time_commands();

	VkBufferImageCopy region{};
	region.bufferOffset = 0;
	region.bufferRowLength = 0;
	region.bufferImageHeight = 0;

	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount = layer_count;

	region.imageOffset = {0, 0, 0};
	region.imageExtent = {
		width,
		height,
		1
	};

	vkCmdCopyBufferToImage(
		command_buffer,
		staging_buffer.get_buffer(),
		destination_image,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1,
		&region
	);

	get_graphics_engine().end_single_time_commands(command_buffer);
}

void GraphicsBufferManager::reserve_buffer(GraphicsBuffer& buffer, uint64_t id, size_t size)
{
	buffer.reserve_slot(id, size);
	update_buffer_stats();
}

void GraphicsBufferManager::free_buffer(GraphicsBuffer& buffer, uint64_t id)
{
	buffer.free_slot(id);
	update_buffer_stats();
}

void GraphicsBufferManager::update_buffer_stats()
{
	const std::vector<std::pair<size_t, size_t>> buffer_capacities =
	{
		{ vertex_buffer.get_filled_capacity(), vertex_buffer.get_capacity() },
		{ index_buffer.get_filled_capacity(), index_buffer.get_capacity() },
		{ uniform_buffer.get_filled_capacity(), uniform_buffer.get_capacity() },
		{ materials_buffer.get_filled_capacity(), materials_buffer.get_capacity() },
		{ mapping_buffer.get_filled_capacity(), mapping_buffer.get_capacity() },
		{ bone_buffer.get_filled_capacity(), bone_buffer.get_capacity() }
	};
	get_graphics_engine().get_gui_manager().update_buffer_capacities(buffer_capacities);
}
