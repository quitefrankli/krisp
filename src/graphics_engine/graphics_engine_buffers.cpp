#include "graphics_engine.hpp"
#include "objects.hpp"


void GraphicsEngine::create_buffer(size_t size, 
								   VkBufferUsageFlags usage_flags, 
								   VkMemoryPropertyFlags memory_flags, 
								   VkBuffer& buffer, 
								   VkDeviceMemory& device_memory)
{
	VkBufferCreateInfo buffer_create_info{};
	buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	buffer_create_info.size = size;
	buffer_create_info.usage = usage_flags;
	buffer_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE; // will only be used in graphics queue

	if (vkCreateBuffer(get_logical_device(), &buffer_create_info, nullptr, &buffer) != VK_SUCCESS)
	{
		throw std::runtime_error("GraphicsEngine::create_buffer: failed to create buffer!");
	}

	VkMemoryRequirements memory_requirements;
	vkGetBufferMemoryRequirements(get_logical_device(), buffer, &memory_requirements);
	
	VkMemoryAllocateInfo memory_allocate_info{};
	memory_allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memory_allocate_info.allocationSize = memory_requirements.size;
	memory_allocate_info.memoryTypeIndex = find_memory_type(memory_requirements.memoryTypeBits, memory_flags);

	// allocate memory, note that this is quite inefficient, the correct way of doing this would be to 
	// allocate 1 giant lump of memory and then use offsets to choose which section to use
	if (vkAllocateMemory(get_logical_device(), &memory_allocate_info, nullptr, &device_memory) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to allocate device buffer memory!");
	}

	// bind memory
	vkBindBufferMemory(get_logical_device(), buffer, device_memory, 0);
}

void GraphicsEngine::create_vertex_buffer(GraphicsEngineObject& object)
{
	// this will need to be updated
	size_t buffer_size = 0;
	for (auto& vertex_set : object.get_vertex_sets())
	{
		buffer_size += vertex_set.size() * sizeof(vertex_set[0]);
	}

	// staging buffer, used for copying the data from staging to vertex buffer
	// the staging buffer is the buffer that can be accessed by CPU
	VkBuffer staging_buffer;
	VkDeviceMemory staging_buffer_memory;
	create_buffer(buffer_size,
				  VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				  // make sure that the memory heap is host coherent
			 	  // this is because the driver may not immediately copy the data into the buffer memory
				  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				  staging_buffer,
				  staging_buffer_memory);

	// the device local buffer is most efficient for GPU access however CPU cannot access it
	// which is why we need a staging buffer in the first place
	create_buffer(buffer_size,
				  VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
				  // device local here means it basically lives on the graphics card hence we can't map it,
				  // but it's alot faster than using COHERENT buffer
				  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				  object.vertex_buffer,
				  object.vertex_buffer_memory);

	// filling the vertex buffer
	// copy the vertex data to the buffer, this is done by mapping the buffer memory into CPU accessible memory with vkMapMemory
	void* data;
	// access a region of the specified memory resource
	vkMapMemory(get_logical_device(), staging_buffer_memory, 0, buffer_size, 0, &data);
	size_t offset = 0;
	for (auto& vertex_set : object.get_vertex_sets())
	{
		size_t size = vertex_set.size() * sizeof(vertex_set[0]);
		memcpy((char*)data + offset, vertex_set.data(), size);
		offset += size;
	}
	vkUnmapMemory(get_logical_device(), staging_buffer_memory);

	// issue the command to copy from staging to device
	copy_buffer(staging_buffer, object.vertex_buffer, buffer_size);

	vkDestroyBuffer(get_logical_device(), staging_buffer, nullptr);
	vkFreeMemory(get_logical_device(), staging_buffer_memory, nullptr);
}

void GraphicsEngine::copy_buffer(VkBuffer src_buffer, VkBuffer dest_buffer, size_t size)
{
	// memory transfer operations are executed using command buffers

	VkCommandBuffer command_buffer = begin_single_time_commands();

	// actual copy command
	VkBufferCopy copy_region{};
	copy_region.size = size;
	vkCmdCopyBuffer(command_buffer, src_buffer, dest_buffer, 1, &copy_region);

	end_single_time_commands(command_buffer);
}