#pragma once

#include "graphics_engine/graphics_engine_base_module.hpp"
#include "graphics_buffer.hpp"
#include "vertex.hpp"
#include "shapes/shape.hpp"
#include "graphics_engine/uniform_buffer_object.hpp"
#include "buffer_map.hpp"


// Manages buffers associated with objects such as vertex buffer
// Memory is virtualised so that the GPU sees a continuous memory space
template<typename GraphicsEngineT>
class GraphicsBufferManager : public GraphicsEngineBaseModule<GraphicsEngineT>
{
public:
	GraphicsBufferManager(GraphicsEngineT& engine);
	virtual ~GraphicsBufferManager() override;

	void reserve_vertex_buffer(uint32_t id, uint32_t size) { vertex_buffer.reserve_slot(id, size); }
	void reserve_index_buffer(uint32_t id, uint32_t size) { index_buffer.reserve_slot(id, size); }
	void reserve_uniform_buffer(uint32_t id, uint32_t size) { uniform_buffer.reserve_slot(id, size); }

	void free_vertex_buffer(uint32_t id) { vertex_buffer.free_slot(id); }
	void free_index_buffer(uint32_t id) { index_buffer.free_slot(id); }
	void free_uniform_buffer(uint32_t id) { uniform_buffer.free_slot(id); }

	uint32_t get_vertex_buffer_offset(uint32_t id) const { return vertex_buffer.get_offset(id); }
	uint32_t get_index_buffer_offset(uint32_t id) const { return index_buffer.get_offset(id); }
	uint32_t get_uniform_buffer_offset(uint32_t id) const { return uniform_buffer.get_offset(id); }

	VkBuffer get_vertex_buffer() const { return vertex_buffer.get_buffer(); }
	VkBuffer get_index_buffer() const { return index_buffer.get_buffer(); }
	VkBuffer get_uniform_buffer() const { return uniform_buffer.get_buffer(); }

	void write_to_vertex_buffer(const std::vector<Vertex>& vertices, uint32_t id);
	void write_to_index_buffer(const std::vector<uint32_t>& indices, uint32_t id);
	void write_to_uniform_buffer(const std::vector<UniformBufferObject>& ubos, uint32_t id);
	void write_to_uniform_buffer(const UniformBufferObject& ubos, uint32_t id);
	// does both vertex and index buffer writing
	void write_shapes_to_buffers(const std::vector<Shape>& shapes, uint32_t id);
	void write_to_mapping_buffer(const std::vector<BufferMapEntry>& entries, uint32_t id);
	void write_to_mapping_buffer(const BufferMapEntry& entry, uint32_t id);

	GraphicsBufferV2 create_buffer(
		size_t size, 
		VkBufferUsageFlags usage_flags, 
		VkMemoryPropertyFlags memory_flags,
		uint32_t alignment = 1);

private:
	void stage_data_to_buffer(
		VkBuffer destination_buffer,
		const uint32_t destination_buffer_offset,
		const uint32_t size,
		const std::function<void(std::byte*)>& write_function);

	// in bytes
	const size_t VERTEX_BUFFER_CAPACITY = sizeof(Vertex) * 2e5;
	const size_t INDEX_BUFFER_CAPACITY = sizeof(uint32_t) * 1e5;
	const size_t UNIFORM_BUFFER_CAPACITY = sizeof(UniformBufferObject) * 1e4;
	const size_t MAPPING_BUFFER_CAPACITY = sizeof(BufferMapEntry) * 1e4;

	const VkBufferUsageFlags VERTEX_BUFFER_USAGE_FLAGS = VK_BUFFER_USAGE_TRANSFER_DST_BIT | 
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
	const VkBufferUsageFlags INDEX_BUFFER_USAGE_FLAGS = VK_BUFFER_USAGE_TRANSFER_DST_BIT |
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
	const VkBufferUsageFlags UNIFORM_BUFFER_USAGE_FLAGS = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	const VkBufferUsageFlags MAPPING_BUFFER_USAGE_FLAGS = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

	const VkMemoryPropertyFlags VERTEX_BUFFER_MEMORY_FLAGS = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	const VkMemoryPropertyFlags INDEX_BUFFER_MEMORY_FLAGS = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	const VkMemoryPropertyFlags UNIFORM_BUFFER_MEMORY_FLAGS = 
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
	const VkMemoryPropertyFlags MAPPING_BUFFER_MEMORY_FLAGS = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

	GraphicsBufferV2 vertex_buffer;
	GraphicsBufferV2 index_buffer;
	GraphicsBufferV2 uniform_buffer;
	// maps object id to starting offset in the vertex, index and uniform buffers
	// unlike the other buffers, entries in this buffer never gets removed
	AppendOnlyGraphicsBuffer mapping_buffer;
};