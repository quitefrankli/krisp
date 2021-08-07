#include "graphics_engine.hpp"

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

	if (vkCreateBuffer(logical_device, &buffer_create_info, nullptr, &buffer) != VK_SUCCESS)
	{
		throw std::runtime_error("GraphicsEngine::create_buffer: failed to create buffer!");
	}

	VkMemoryRequirements memory_requirements;
	vkGetBufferMemoryRequirements(logical_device, buffer, &memory_requirements);
	
	// graphics cards offer different types of memory to allocate from, each type of memory varies
	// in therms of allowed operations and performance characteristics
	auto find_memory_type = [this](uint32_t type_filter, VkMemoryPropertyFlags flags)
	{
		VkPhysicalDeviceMemoryProperties memory_properties;
		vkGetPhysicalDeviceMemoryProperties(this->physicalDevice, &memory_properties);

		for (uint32_t i = 0; i < memory_properties.memoryTypeCount; i++)
		{
			if ((type_filter & (1 << i)) && ((memory_properties.memoryTypes[i].propertyFlags & flags) == flags))
			{
				return i;
			}
		}

		throw std::runtime_error("failed to find suitable memory type!");
	};

	VkMemoryAllocateInfo memory_allocate_info{};
	memory_allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memory_allocate_info.allocationSize = memory_requirements.size;
	memory_allocate_info.memoryTypeIndex = find_memory_type(memory_requirements.memoryTypeBits, memory_flags);

	// allocate memory, note that this is quite inefficient, the correct way of doing this would be to 
	// allocate 1 giant lump of memory and then use offsets to choose which section to use
	if (vkAllocateMemory(logical_device, &memory_allocate_info, nullptr, &device_memory) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to allocate device buffer memory!");
	}

	// bind memory
	vkBindBufferMemory(logical_device, buffer, device_memory, 0);
}

void GraphicsEngine::create_vertex_buffer()
{
	if (vertices.size() == 0)
	{
		throw std::runtime_error("vertices cannot be 0");
	}

	const size_t buffer_size = sizeof(vertices[0]) * vertices.size();

	// staging buffer, used for copying the data from staging to vertex buffer
	VkBuffer staging_buffer;
	VkDeviceMemory staging_buffer_memory;
	create_buffer(buffer_size,
				  VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				  // make sure that the memory heap is host coherent
			 	  // this is because the driver may not immediately copy the data into the buffer memory
				  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				  staging_buffer,
				  staging_buffer_memory);

	// vertex buffer
	create_buffer(buffer_size,
				  VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
				  // device local here means it basically lives on the graphics card hence we can't map it,
				  // but it's alot faster than using COHERENT buffer
				  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				  vertex_buffer,
				  vertex_buffer_memory);

	// filling the vertex buffer
	// copy the vertex data to the buffer, this is done by mapping the buffer memory into CPU accessible memory with vkMapMemory
	void* data;
	// access a region of the specified memory resource
	vkMapMemory(logical_device, staging_buffer_memory, 0, buffer_size, 0, &data);
	memcpy(data, vertices.data(), static_cast<size_t>(buffer_size));
	vkUnmapMemory(logical_device, staging_buffer_memory);

	// issue the command to copy from staging to device
	copy_buffer(staging_buffer, vertex_buffer, buffer_size);

	vkDestroyBuffer(logical_device, staging_buffer, nullptr);
	vkFreeMemory(logical_device, staging_buffer_memory, nullptr);
}

void GraphicsEngine::copy_buffer(VkBuffer src_buffer, VkBuffer dest_buffer, size_t size)
{
	// memory transfer operations are executed using command buffers

	VkCommandBufferAllocateInfo alloc_info{};
	alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	alloc_info.commandPool = command_pool;
	alloc_info.commandBufferCount = 1;

	VkCommandBuffer command_buffer;
	vkAllocateCommandBuffers(logical_device, &alloc_info, &command_buffer);

	// start recording the command buffer
	VkCommandBufferBeginInfo begin_info{};
	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	// only going to use the command buffer once so we should let driver know our intent
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	vkBeginCommandBuffer(command_buffer, &begin_info);

	// actual copy command
	VkBufferCopy copyRegion{};
	copyRegion.srcOffset = 0; // Optional
	copyRegion.dstOffset = 0; // Optional
	copyRegion.size = size;
	vkCmdCopyBuffer(command_buffer, src_buffer, dest_buffer, 1, &copyRegion);

	vkEndCommandBuffer(command_buffer); // stop recording commands

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &command_buffer;

	// note that unlike draw stage, we don't need to wait for anything here except for the queue to become idle
	vkQueueSubmit(graphics_queue, 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(graphics_queue);

	vkFreeCommandBuffers(logical_device, command_pool, 1, &command_buffer);
}