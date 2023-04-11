#pragma once

#include "graphics_engine_base_module.hpp"
#include "vulkan_wrappers.hpp"

#include <vulkan/vulkan.hpp>


VkTransformMatrixKHR glm_to_vk(const glm::mat4& matrix);

template<typename GraphicsEngineT>
class GraphicsEngineSwapChain;

template<typename GraphicsEngineT>
class GraphicsEngineObject;

// Note that frame refers to swap_chain frame and not actual frames
template<typename GraphicsEngineT>
class GraphicsEngineRayTracing : public GraphicsEngineBaseModule<GraphicsEngineT>
{
public:
	GraphicsEngineRayTracing(GraphicsEngineT& engine);
	GraphicsEngineRayTracing(const GraphicsEngineRayTracing&) = delete;
	~GraphicsEngineRayTracing();

	struct BlasInput
	{
		// Data used to build acceleration structure geometry
		std::vector<VkAccelerationStructureGeometryKHR>       asGeometry;
		std::vector<VkAccelerationStructureBuildRangeInfoKHR> asBuildOffsetInfo;
		VkBuildAccelerationStructureFlagsKHR                  flags{0};
	};

	struct AccelerationStructure
	{
		VkAccelerationStructureKHR accel = VK_NULL_HANDLE;
		// TODO: might need to clean the buffer and device memory up
		VkBuffer buffer = VK_NULL_HANDLE;
		VkDeviceMemory memory = VK_NULL_HANDLE;
	};

	// TODO: make this RAII
	void cleanup_acceleration_structure(AccelerationStructure& as);

	struct BuildAccelerationStructure
	{
		VkAccelerationStructureBuildGeometryInfoKHR buildInfo{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR};
		VkAccelerationStructureBuildSizesInfoKHR sizeInfo{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
		const VkAccelerationStructureBuildRangeInfoKHR* rangeInfo;
		AccelerationStructure as;  // result acceleration structure
		AccelerationStructure cleanupAS;
  	};

	void update_acceleration_structures();

	void update_tlas2();
	VkAccelerationStructureKHR get_tlas() const { return top_as.accel; }

public:
	// sbt = shader binding table
	GraphicsBuffer sbt_buffer;
	VkStridedDeviceAddressRegionKHR raygen_sbt_region{};
	VkStridedDeviceAddressRegionKHR raymiss_sbt_region{};
	VkStridedDeviceAddressRegionKHR rayhit_sbt_region{};
	VkStridedDeviceAddressRegionKHR callable_sbt_region{};

private:
	BlasInput object_to_blas(const GraphicsEngineObject<GraphicsEngineT>& object, VkBuildAccelerationStructureFlagsKHR flags);
	// blas = bottom level acceleration struction
	// tlas = top level acceleration structure
	void update_blas();
	void update_tlas();

	//--------------------------------------------------------------------------------------------------
	// Creating the bottom level acceleration structure for all indices of `buildAs` vector.
	// The array of BuildAccelerationStructure was created in buildBlas and the vector of
	// indices limits the number of BLAS to create at once. This limits the amount of
	// memory needed when compacting the BLAS.
	void cmd_create_blas(
		VkCommandBuffer cmd_buf,
		std::vector<uint32_t> indices,
		std::vector<BuildAccelerationStructure>& build_as,
		VkDeviceAddress scratch_address,
		VkQueryPool query_pool);

	//--------------------------------------------------------------------------------------------------
	// Create and replace a new acceleration structure and buffer based on the size retrieved by the
	// Query.
	void cmd_compact_blas(
		VkCommandBuffer cmd_buf,
		std::vector<uint32_t> indices,
		std::vector<BuildAccelerationStructure>& build_as,
		VkQueryPool query_pool);

	void cmd_create_tlas(
		VkCommandBuffer cmd_buf,
		uint32_t nInstances,
		VkDeviceAddress inst_buffer_addr,
		GraphicsBuffer& scratch_buffer,
		VkBuildAccelerationStructureFlagsKHR flags,
		bool update);

	AccelerationStructure create_acceleration_structure(VkAccelerationStructureCreateInfoKHR& create_info);

	//--------------------------------------------------------------------------------------------------
	// Return the device address of a Blas previously created.
	//
	VkDeviceAddress getBlasDeviceAddress(uint32_t blasId);

	void build_tlas(
		const std::vector<VkAccelerationStructureInstanceKHR>& instances,
		VkBuildAccelerationStructureFlagsKHR flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
		bool update = false);

	void create_shader_binding_table();

	void destroy_as(AccelerationStructure& as);

private:
	std::vector<AccelerationStructure> bottom_as;
	AccelerationStructure top_as;
	std::vector<VkAccelerationStructureInstanceKHR> tlas_instances;

private:
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_graphics_engine;
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_logical_device;
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_physical_device;
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_instance;
	using GraphicsEngineBaseModule<GraphicsEngineT>::create_buffer;
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_num_swapchain_frames;
	using GraphicsEngineBaseModule<GraphicsEngineT>::should_destroy;
};