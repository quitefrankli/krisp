#pragma once

#include "graphics_engine.hpp"
#include "objects/object.hpp"


template<typename GameEngineT>
void GraphicsEngine<GameEngineT>::create_buffer(size_t size, 
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
	
	VkMemoryAllocateInfo memory_allocate_info{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
	memory_allocate_info.allocationSize = memory_requirements.size;
	memory_allocate_info.memoryTypeIndex = find_memory_type(memory_requirements.memoryTypeBits, memory_flags);

	VkMemoryAllocateFlagsInfo memory_allocate_flags_info{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO};
	memory_allocate_flags_info.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;

	memory_allocate_info.pNext = &memory_allocate_flags_info;

	// allocate memory, note that this is quite inefficient, the correct way of doing this would be to 
	// allocate 1 giant lump of memory and then use offsets to choose which section to use
	if (vkAllocateMemory(get_logical_device(), &memory_allocate_info, nullptr, &device_memory) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to allocate device buffer memory!");
	}

	// bind memory
	vkBindBufferMemory(get_logical_device(), buffer, device_memory, 0);
}

template<typename GameEngineT>
void GraphicsEngine<GameEngineT>::create_object_buffers(GraphicsEngineObject<GraphicsEngine>& object)
{
	GraphicsBuffer vertex_buffer = create_buffer_from_data(
		nullptr,
		object.get_num_unique_vertices() * sizeof(Vertex),
		// transfer dst means that the buffer can be used as a destination for transfer operations
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		// device local here means it basically lives on the graphics card hence we can't map it,
		// but it's alot faster than using COHERENT buffer
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		[&object](void* data) {
			size_t offset = 0;
			for (const auto& shape : object.get_shapes())
			{
				size_t size = shape.vertices.size() * sizeof(typename decltype(shape.vertices)::value_type);
				memcpy((char*)data + offset, shape.vertices.data(), size);
				offset += size;
			}
		}
	);

	GraphicsBuffer index_buffer = create_buffer_from_data(
		nullptr,
		object.get_num_vertex_indices() * sizeof(uint32_t),
		// transfer dst means that the buffer can be used as a destination for transfer operations
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		// device local here means it basically lives on the graphics card hence we can't map it,
		// but it's alot faster than using COHERENT buffer
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		[&](void* data) {
			size_t offset = 0;
			for (const auto& shape : object.get_shapes())
			{
				size_t size = shape.indices.size() * sizeof(typename decltype(shape.indices)::value_type);
				memcpy((char*)data + offset, shape.indices.data(), size);
				offset += size;
			}
		}
	);

	object.vertex_buffer = vertex_buffer.buffer;
	object.vertex_buffer_memory = vertex_buffer.memory;
	object.index_buffer = index_buffer.buffer;
	object.index_buffer_memory = index_buffer.memory;
}

template<typename GameEngineT>
void GraphicsEngine<GameEngineT>::copy_buffer(VkBuffer src_buffer, VkBuffer dest_buffer, size_t size)
{
	// memory transfer operations are executed using command buffers

	VkCommandBuffer command_buffer = begin_single_time_commands();

	// actual copy command
	VkBufferCopy copy_region{};
	copy_region.size = size;
	vkCmdCopyBuffer(command_buffer, src_buffer, dest_buffer, 1, &copy_region);

	end_single_time_commands(command_buffer);
}

template<typename GameEngineT>
GraphicsBuffer GraphicsEngine<GameEngineT>::create_buffer(size_t size,
                                                                 VkBufferUsageFlags usage_flags,
                                                                 VkMemoryPropertyFlags memory_flags)
{
	GraphicsBuffer buffer;
	create_buffer(size, usage_flags, memory_flags, buffer.buffer, buffer.memory);
	
	return buffer;
}

template<typename GameEngineT>
GraphicsBuffer GraphicsEngine<GameEngineT>::create_buffer_from_data(
	const void* data,
	size_t size,
	VkBufferUsageFlags usage_flags,
	VkMemoryPropertyFlags memory_flags,
	const std::function<void(void* dest)> copy_func)
{
	// create staging buffer, used for copying the data from staging to vertex buffer
	// the staging buffer is the buffer that can be accessed by CPU
	GraphicsBuffer staging_buffer = create_buffer(
		size,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		// make sure that the memory heap is host coherent
		// this is because the driver may not immediately copy the data into the buffer memory
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	// fill the staging buffer
	void* mapped_data;
	vkMapMemory(get_logical_device(), staging_buffer.memory, 0, size, 0, &mapped_data);
	if (copy_func)
	{
		copy_func(mapped_data);
	} else 
	{
		std::memcpy(mapped_data, data, size);
	}
	vkUnmapMemory(get_logical_device(), staging_buffer.memory);

	// now create the actual buffer we want to use. AKA the destination buffer
	GraphicsBuffer buffer = create_buffer(size, usage_flags, memory_flags);

	// issue the command to copy from staging to device
	copy_buffer(staging_buffer.buffer, buffer.buffer, size);

	// clean up staging buffer
	vkDestroyBuffer(get_logical_device(), staging_buffer.buffer, nullptr);
	vkFreeMemory(get_logical_device(), staging_buffer.memory, nullptr);

	return buffer;
}

template<typename GameEngineT>
RenderingAttachment GraphicsEngine<GameEngineT>::create_depth_buffer_attachment()
{
	const VkFormat depth_format = find_depth_format();
	const auto extent = get_extent();
	RenderingAttachment depth_attachment;
	create_image(
		extent.width,
		extent.height,
		depth_format,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		depth_attachment.image,
		depth_attachment.image_memory,
		get_msaa_samples());
	depth_attachment.image_view = create_image_view(
		depth_attachment.image, 
		depth_format, 
		VK_IMAGE_ASPECT_DEPTH_BIT);

	return depth_attachment;
}

template<typename GameEngineT>
VkFormat GraphicsEngine<GameEngineT>::find_depth_format()
{
	if (!depth_format)
	{
		const auto find_supported_format = [&](
			std::vector<VkFormat> candidates, 
			VkImageTiling tiling, 
			VkFormatFeatureFlags features)
		{
			for (VkFormat format : candidates)
			{
				VkFormatProperties props;
				vkGetPhysicalDeviceFormatProperties(get_physical_device(), format, &props);
				if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features)
				{
					return format;
				}

				if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features)
				{
					return format;
				}
			}

			throw std::runtime_error("failed to find supported format!");
		};

		depth_format = find_supported_format(
			{VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
			VK_IMAGE_TILING_OPTIMAL,
			VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
	}

	return depth_format.value();
}
