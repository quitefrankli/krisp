#pragma once

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>

#include <cstddef>
#include <map>
#include <string>
#include <unordered_map>
#include <utility>

class GraphicsBuffer
{
public:
	using offset_t = uint32_t;
	using SlotID = uint64_t;

	GraphicsBuffer(
		VkBuffer buffer,
		VmaAllocation allocation,
		VmaAllocator allocator,
		uint32_t capacity,
		uint32_t alignment = 1,
		std::string name = "");
	GraphicsBuffer(const GraphicsBuffer&) = delete;
	GraphicsBuffer& operator=(const GraphicsBuffer&) = delete;
	GraphicsBuffer(GraphicsBuffer&& other) noexcept;
	GraphicsBuffer& operator=(GraphicsBuffer&& other) noexcept;
	~GraphicsBuffer();

	void destroy();

	struct Slot
	{
		uint32_t offset;
		uint32_t size;
		uint32_t capacity;
	};

	void free_slot(SlotID slot_id);
	offset_t reserve_slot(SlotID id, uint32_t size);
	void* map();
	void unmap();
	void flush(VkDeviceSize offset = 0, VkDeviceSize size = VK_WHOLE_SIZE);
	void invalidate(VkDeviceSize offset = 0, VkDeviceSize size = VK_WHOLE_SIZE);
	std::byte* map_slot(SlotID id);
	void unmap_slot();

	size_t get_capacity() const { return capacity; }
	size_t get_filled_capacity() const { return filled_capacity; }
	offset_t get_offset(SlotID id) const;
	VkBuffer get_buffer() const { return buffer; }
	VmaAllocation get_allocation() const { return allocation; }
	const std::string& get_name() const { return name; }
	Slot get_slot(SlotID id) const;
	bool has_slot(SlotID id) const { return filled_slots.contains(id); }

private:
	std::unordered_map<SlotID, Slot> filled_slots;
	std::map<uint32_t, Slot> free_slots;

	VkBuffer buffer = VK_NULL_HANDLE;
	VmaAllocation allocation = VK_NULL_HANDLE;
	VmaAllocator allocator = VK_NULL_HANDLE;
	void* mapped_data = nullptr;
	VkDeviceSize mapped_slot_offset = 0;
	VkDeviceSize mapped_slot_size = 0;
	size_t filled_capacity = 0;
	size_t capacity;
	uint32_t alignment;
	std::string name;
};

class AppendOnlyGraphicsBuffer
{
public:
	AppendOnlyGraphicsBuffer(GraphicsBuffer&& buffer, uint32_t slot_size) :
		buffer(std::move(buffer)),
		slot_size(slot_size)
	{
	}

	void destroy() { buffer.destroy(); }
	void decrease_free_capacity(size_t size) { filled_capacity += size; }
	uint32_t get_slot_offset(uint32_t slot_num) const { return slot_num * slot_size; }
	VkBuffer get_buffer() const { return buffer.get_buffer(); }
	VmaAllocation get_allocation() const { return buffer.get_allocation(); }
	size_t get_capacity() const { return buffer.get_capacity(); }
	size_t get_filled_capacity() const { return filled_capacity; }

private:
	const uint32_t slot_size;
	size_t filled_capacity = 0;
	GraphicsBuffer buffer;
};
