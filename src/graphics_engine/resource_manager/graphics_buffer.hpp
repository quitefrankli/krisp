#pragma once

#include <vulkan/vulkan.hpp>

#include <unordered_map>
#include <map>


class GraphicsBuffer
{
public:
	using offset_t = uint32_t;

	GraphicsBuffer(VkBuffer buffer, VkDeviceMemory memory, uint32_t capacity, uint32_t alignment = 1);
	GraphicsBuffer(const GraphicsBuffer&) = delete;
	GraphicsBuffer(GraphicsBuffer&& other) noexcept;
	~GraphicsBuffer();

	void destroy(VkDevice device);

	// potential slots for different objects etc.
	struct Slot
	{
		uint32_t offset;
		uint32_t size;
		uint32_t capacity; // this is only for alignment requirements, for real data size refer to above
	};

	void free_slot(uint32_t slot_id);
	offset_t reserve_slot(uint32_t id, uint32_t size);
	std::byte* map_slot(uint32_t id, VkDevice device);
	void unmap_slot(VkDevice device);

	uint32_t get_capacity() const { return capacity; }
	offset_t get_offset(uint32_t id) const;
	VkBuffer get_buffer() const { return buffer; }
	VkDeviceMemory get_memory() const { return memory; }
	Slot get_slot(uint32_t id) const;

private:
	// map of id to slot, id is typically object-id
	std::unordered_map<uint32_t, Slot> filled_slots;

	// map of offset to slot, offset is the offset in the buffer
	// these slots are unused and to ready to be picked
	std::map<uint32_t, Slot> free_slots;

	VkBuffer buffer = nullptr;
	VkDeviceMemory memory = nullptr;
	const uint32_t capacity;
	const uint32_t alignment;
};

// This buffer version is append only, it can only grow and never shrink or modify contents
class AppendOnlyGraphicsBuffer
{
public:
	AppendOnlyGraphicsBuffer(GraphicsBuffer&& buffer, uint32_t slot_size) :
		buffer(std::move(buffer)),
		slot_size(slot_size)
	{
	}

	void destroy(VkDevice device)
	{
		buffer.destroy(device);
	}

	uint32_t get_slot_offset(uint32_t slot_num) const { return slot_num * slot_size; }

	VkBuffer get_buffer() const { return buffer.get_buffer(); }
	VkDeviceMemory get_memory() const { return buffer.get_memory(); }

private:
	const uint32_t slot_size;

	GraphicsBuffer buffer;
};