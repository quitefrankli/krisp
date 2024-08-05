#pragma once

#include "graphics_engine/constants.hpp"
#include "graphics_engine/graphics_engine_base_module.hpp"
#include "graphics_buffer.hpp"
#include "shared_data_structures.hpp"
#include "identifications.hpp"
#include "graphics_engine/graphics_engine_object.hpp"


class Mesh;

// Manages buffers associated with objects such as vertex buffer
// Memory is virtualised so that the GPU sees a continuous memory space
class GraphicsBufferManager : public GraphicsEngineBaseModule
{
public:
	GraphicsBufferManager(GraphicsEngine& engine);
	virtual ~GraphicsBufferManager() override;

	void reserve_uniform_buffer(EntityFrameID id, size_t size) { reserve_buffer(uniform_buffer, id.get_underlying(), size); }
	void reserve_buffer(SkeletonFrameID id, size_t size) { reserve_buffer(bone_buffer, id.get_underlying(), size); }

	void free_uniform_buffer(EntityFrameID id) { free_buffer(uniform_buffer, id.get_underlying()); }
	void free_buffer(MeshID id) { free_buffer(vertex_buffer, id.get_underlying()); free_buffer(index_buffer, id.get_underlying()); }
	void free_buffer(MaterialID id) { free_buffer(materials_buffer, id.get_underlying()); }
	void free_buffer(SkeletonFrameID id) { free_buffer(bone_buffer, id.get_underlying()); }

	size_t get_vertex_buffer_offset(MeshID id) const { return vertex_buffer.get_offset(id.get_underlying()); }
	size_t get_index_buffer_offset(MeshID id) const { return index_buffer.get_offset(id.get_underlying()); }
	size_t get_uniform_buffer_offset(EntityFrameID id) const { return uniform_buffer.get_offset(id.get_underlying()); }
	size_t get_buffer_offset(MaterialID id) const { return materials_buffer.get_offset(id.get_underlying()); }
	size_t get_buffer_offset(SkeletonFrameID id) const { return bone_buffer.get_offset(id.get_underlying()); }
	size_t get_global_uniform_buffer_offset(uint32_t id) const { return global_uniform_buffer.get_offset(id); }

	VkBuffer get_vertex_buffer() const { return vertex_buffer.get_buffer(); }
	VkBuffer get_index_buffer() const { return index_buffer.get_buffer(); }
	VkBuffer get_uniform_buffer() const { return uniform_buffer.get_buffer(); }
	VkBuffer get_materials_buffer() const { return materials_buffer.get_buffer(); }
	VkBuffer get_mapping_buffer() const { return mapping_buffer.get_buffer(); }
	VkBuffer get_global_uniform_buffer() const { return global_uniform_buffer.get_buffer(); }
	VkBuffer get_bone_buffer() const { return bone_buffer.get_buffer(); }

	VkDeviceMemory get_global_uniform_buffer_memory() const { return global_uniform_buffer.get_memory(); }

	// does both vertex and index buffer writing
	void write_to_buffer(MeshID id, const Mesh& mesh);
	void write_to_buffer(MaterialID id, const SDS::MaterialData& material);
	void write_to_buffer(SkeletonFrameID id, const std::vector<SDS::Bone>& bones);
	void write_to_uniform_buffer(EntityFrameID id, const SDS::ObjectData& ubos);
	void write_to_global_uniform_buffer(uint32_t id, const SDS::GlobalData& ubo);
	void write_to_mapping_buffer(ObjectID id, const SDS::BufferMapEntry& entry);

	GraphicsBuffer::Slot get_vertex_buffer_slot(MeshID id) const { return vertex_buffer.get_slot(id.get_underlying()); }
	GraphicsBuffer::Slot get_index_buffer_slot(MeshID id) const { return index_buffer.get_slot(id.get_underlying()); }
	GraphicsBuffer::Slot get_uniform_buffer_slot(EntityFrameID id) const { return uniform_buffer.get_slot(id.get_underlying()); }
	GraphicsBuffer::Slot get_buffer_slot(MaterialID id) const { return materials_buffer.get_slot(id.get_underlying()); }
	GraphicsBuffer::Slot get_buffer_slot(SkeletonFrameID id) const { return bone_buffer.get_slot(id.get_underlying()); }

	virtual GraphicsBuffer create_buffer(
		size_t size, 
		VkBufferUsageFlags usage_flags, 
		VkMemoryPropertyFlags memory_flags,
		uint32_t alignment = 1) override;

	// Deprecated dont use this
	void create_buffer_deprecated(
		size_t size,
		VkBufferUsageFlags usage_flags,
		VkMemoryPropertyFlags memory_flags,
		VkBuffer& buffer,
		VkDeviceMemory& buffer_memory);

	void stage_data_to_buffer(
		VkBuffer destination_buffer,
		const uint32_t destination_buffer_offset,
		const uint32_t size,
		const std::function<void(std::byte*)>& write_function);
	
	void stage_data_to_image(
		VkImage destination_image,
		const uint32_t width,
		const uint32_t height,
		const size_t size,
		const std::function<void(std::byte*)>& write_function,
		const uint32_t layer_count = 1); // for cubemaps

public:
	static constexpr size_t NUM_EXPECTED_OBJECTS = 1e3;
	static constexpr size_t NUM_EXPECTED_FRAMES = 3;
	static constexpr size_t NUM_EXPECTED_RENDERABLES = NUM_EXPECTED_OBJECTS * 2;

	// in bytes
	// takes average size different vertex types
	static constexpr size_t VERTEX_BUFFER_CAPACITY = (sizeof(SDS::ColorVertex) + sizeof(SDS::TexVertex)) * 1e5;
	static constexpr size_t INDEX_BUFFER_CAPACITY = sizeof(uint32_t) * 1e6;
	static constexpr size_t UNIFORM_BUFFER_CAPACITY = sizeof(SDS::ObjectData) * NUM_EXPECTED_OBJECTS * NUM_EXPECTED_FRAMES;
	static constexpr size_t MATERIALS_BUFFER_CAPACITY = sizeof(SDS::MaterialData) * NUM_EXPECTED_RENDERABLES;
	static constexpr size_t GLOBAL_UNIFORM_BUFFER_CAPACITY = sizeof(SDS::GlobalData) * CSTS::NUM_EXPECTED_SWAPCHAIN_IMAGES * 100; // 100 is here to get around the min uniform buffer alignment requirement
	static constexpr size_t MAPPING_BUFFER_CAPACITY = sizeof(SDS::BufferMapEntry) * NUM_EXPECTED_OBJECTS * 10;
	static constexpr size_t BONE_BUFFER_CAPACITY = sizeof(SDS::Bone) * 500 * NUM_EXPECTED_FRAMES;
	static constexpr size_t INITIAL_STAGING_BUFFER_CAPACITY = 1e4; // staging buffer capacity dynamically grows

private:
	void reserve_buffer(GraphicsBuffer& buffer, uint64_t id, size_t size);
	void free_buffer(GraphicsBuffer& buffer, uint64_t id);
	void update_buffer_stats();

	static constexpr VkBufferUsageFlags VERTEX_BUFFER_USAGE_FLAGS = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | 
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	static constexpr VkBufferUsageFlags INDEX_BUFFER_USAGE_FLAGS = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | 
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	static constexpr VkBufferUsageFlags UNIFORM_BUFFER_USAGE_FLAGS = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	static constexpr VkBufferUsageFlags MATERIALS_BUFFER_USAGE_FLAGS = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	static constexpr VkBufferUsageFlags GLOBAL_UNIFORM_BUFFER_USAGE_FLAGS = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	static constexpr VkBufferUsageFlags BONE_BUFFER_USAGE_FLAGS = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	static constexpr VkBufferUsageFlags MAPPING_BUFFER_USAGE_FLAGS = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	static constexpr VkBufferUsageFlags STAGING_BUFFER_USAGE_FLAGS = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

	static constexpr VkMemoryPropertyFlags VERTEX_BUFFER_MEMORY_FLAGS = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	static constexpr VkMemoryPropertyFlags INDEX_BUFFER_MEMORY_FLAGS = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	static constexpr VkMemoryPropertyFlags UNIFORM_BUFFER_MEMORY_FLAGS = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
	static constexpr VkMemoryPropertyFlags MATERIALS_BUFFER_MEMORY_FLAGS = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	static constexpr VkMemoryPropertyFlags GLOBAL_UNIFORM_BUFFER_MEMORY_FLAGS = UNIFORM_BUFFER_MEMORY_FLAGS;
	static constexpr VkMemoryPropertyFlags MAPPING_BUFFER_MEMORY_FLAGS = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	static constexpr VkMemoryPropertyFlags BONE_BUFFER_MEMORY_FLAGS = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
	static constexpr VkMemoryPropertyFlags STAGING_BUFFER_MEMORY_FLAGS = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

	GraphicsBuffer vertex_buffer;
	GraphicsBuffer index_buffer;
	GraphicsBuffer uniform_buffer;
	GraphicsBuffer materials_buffer;
	GraphicsBuffer global_uniform_buffer;
	GraphicsBuffer staging_buffer;
	GraphicsBuffer bone_buffer;
	// maps object id to starting offset in the vertex, index and uniform buffers
	// unlike the other buffers, entries in this buffer never gets removed
	AppendOnlyGraphicsBuffer mapping_buffer;
};