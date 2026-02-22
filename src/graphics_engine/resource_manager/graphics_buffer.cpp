#include "graphics_buffer.hpp"

#include <cstddef>
#include <utility>


GraphicsBuffer::GraphicsBuffer(
	VkBuffer buffer,
	VkDeviceMemory memory,
	uint32_t capacity,
	uint32_t alignment,
	std::string name) :
	buffer(buffer),
	memory(memory),
	capacity(capacity),
	alignment(alignment),
	name(std::move(name))
{
	Slot slot;
	slot.offset = 0;
	slot.size = capacity;
	slot.capacity = capacity;
	free_slots[slot.offset] = slot;
}

GraphicsBuffer::GraphicsBuffer(GraphicsBuffer&& other) noexcept :
	buffer(other.buffer),
	memory(other.memory),
	capacity(other.capacity),
	alignment(other.alignment),
	name(std::move(other.name)),
	filled_slots(std::move(other.filled_slots)),
	free_slots(std::move(other.free_slots))
{
	other.buffer = nullptr;
	other.memory = nullptr;
}

GraphicsBuffer::~GraphicsBuffer()
{
	assert(!buffer && !memory);
}

void GraphicsBuffer::destroy(VkDevice device)
{
	if (buffer)
	{
		vkDestroyBuffer(device, buffer, nullptr);
	}
	if (memory)
	{
		vkFreeMemory(device, memory, nullptr);
	}

	buffer = nullptr;
	memory = nullptr;
}

void GraphicsBuffer::free_slot(uint32_t slot_id)
{
	auto it = filled_slots.find(slot_id);
	if (it == filled_slots.end())
	{
		throw std::runtime_error("GraphicsBuffer::free_slot: Slot not found!");
	}

	Slot slot = it->second;
	slot.size = slot.capacity;
	filled_slots.erase(it);
	filled_capacity -= slot.capacity;

	//
	// Now do some housekeeping
	//
	
	// Regarding interusage of size and capacity, in free_slots size==capacity so we use size only
	// in filled_slots size!=capacity and capacity is more important

	// check if there is a slot after, if so merge it
	auto slot_after = free_slots.lower_bound(slot.offset);
	if (slot_after != free_slots.end() && slot_after->second.offset == slot.offset + slot.size)
	{
		slot.size += slot_after->second.size;
	} else
	{
		slot_after = free_slots.end();
	}

	// check if there is a slot before, if so merge it
	auto slot_before = slot_after != free_slots.begin() ? std::prev(slot_after) : free_slots.end();
	if (slot_before != free_slots.end() && 
		slot_before->second.offset + slot_before->second.size == slot.offset)
	{
		slot.offset = slot_before->second.offset;
		slot.size += slot_before->second.size;
	} else
	{
		slot_before = free_slots.end();
	}
	
	if (slot_after != free_slots.end())
	{
		free_slots.erase(slot_after);
	}

	if (slot_before != free_slots.end())
	{
		free_slots.erase(slot_before);
	}

	free_slots[slot.offset] = slot;
}

GraphicsBuffer::offset_t GraphicsBuffer::reserve_slot(uint32_t id, uint32_t size)
{
	if (filled_slots.find(id) != filled_slots.end())
	{
		throw std::runtime_error("Slot already exists!");
	}

	// for alignment, we need to round up the size
	// nice trick that only works with powers of 2: https://stackoverflow.com/questions/3407012/rounding-up-to-the-nearest-multiple-of-a-number
	assert(alignment > 0 && (alignment & (alignment - 1)) == 0); // alignment must be a power of 2
	const uint32_t slot_capacity = (size + alignment - 1) & ~(alignment - 1);

	// alternatively a more generic one that works for non powers of two
	// const uint32_t slot_capacity = ((size + alignment - 1) / alignment) * alignment;

	// find the first slot that fits
	auto it = std::find_if(free_slots.begin(), free_slots.end(), 
		[slot_capacity](const auto& slot) { return slot.second.size >= slot_capacity; });

	if (it == free_slots.end())
	{
		throw std::runtime_error("No free slot found!");
	}

	Slot slot = it->second;
	free_slots.erase(it);

	// if the slot is bigger than needed, split it
	if (slot.size > slot_capacity)
	{
		Slot new_free_slot;
		new_free_slot.offset = slot.offset + slot_capacity;
		new_free_slot.size = slot.size - slot_capacity;
		free_slots[new_free_slot.offset] = new_free_slot;
	}

	slot.size = size;
	slot.capacity = slot_capacity;
	filled_slots[id] = slot;
	filled_capacity += slot_capacity;

	return slot.offset;
}

GraphicsBuffer::offset_t GraphicsBuffer::get_offset(uint32_t id) const
{
	auto it = filled_slots.find(id);
	if (it == filled_slots.end())
	{
		throw std::runtime_error("GraphicsBuffer::get_offset: Slot not found!");
	}

	return it->second.offset;
}

GraphicsBuffer::Slot GraphicsBuffer::get_slot(uint32_t id) const
{
	auto it = filled_slots.find(id);
	if (it == filled_slots.end())
	{
		throw std::runtime_error("GraphicsBuffer::get_slot: Slot not found!");
	}

	return it->second;
}

std::byte* GraphicsBuffer::map_slot(uint32_t id, VkDevice device)
{
	auto it = filled_slots.find(id);
	if (it == filled_slots.end())
	{
		throw std::runtime_error("GraphicsBuffer::map_slot: Slot not found!");
	}

	std::byte* mapped_memory = nullptr;
	if (vkMapMemory(
		device, 
		memory, 
		it->second.offset, 
		it->second.size, 
		0, 
		reinterpret_cast<void**>(&mapped_memory)) != VK_SUCCESS || !mapped_memory)
	{
		throw std::runtime_error("GraphicsEngine::write_to_uniform_buffer: failed to map memory!");
	}

	return mapped_memory;
}

void GraphicsBuffer::unmap_slot(VkDevice device)
{
	vkUnmapMemory(device, memory);
}
