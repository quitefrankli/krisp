#pragma once

#include "raytracing.hpp"
#include "vertex.hpp"
#include "utility.hpp"

#include <quill/Quill.h>

#include <numeric>


VkTransformMatrixKHR glm_to_vk(const glm::mat4& matrix)
{
	VkTransformMatrixKHR vk_matrix;
	std::memcpy(vk_matrix.matrix, &matrix, sizeof(vk_matrix.matrix));
	return vk_matrix;
}


template<typename GraphicsEngineT>
inline GraphicsEngineRayTracing<GraphicsEngineT>::GraphicsEngineRayTracing(
	GraphicsEngineT& engine) :
	offscreen_renderpass(engine),
	GraphicsEngineBaseModule<GraphicsEngineT>(engine)
{
	// update_blas();
	// update_tlas();
}

template<typename GraphicsEngineT>
inline GraphicsEngineRayTracing<GraphicsEngineT>::~GraphicsEngineRayTracing()
{
}

template<typename GraphicsEngineT>
void GraphicsEngineRayTracing<GraphicsEngineT>::cleanup_acceleration_structure(AccelerationStructure& as)
{
	LOAD_VK_FUNCTION(vkDestroyAccelerationStructureKHR)(get_logical_device(), as.accel, nullptr);
	vkDestroyBuffer(get_logical_device(), as.buffer, nullptr);
	vkFreeMemory(get_logical_device(), as.memory, nullptr);
	as = AccelerationStructure{};
}

template<typename GraphicsEngineT>
inline void GraphicsEngineRayTracing<GraphicsEngineT>::update_acceleration_structures()
{
	update_blas();
	update_tlas();
}

template<typename GraphicsEngineT>
typename GraphicsEngineRayTracing<GraphicsEngineT>::BlasInput GraphicsEngineRayTracing<GraphicsEngineT>::object_to_blas(
	const GraphicsEngineObject<GraphicsEngineT>& object)
{
	// BLAS builder requires raw device addresses.
	VkDeviceAddress vertex_address = get_graphics_engine().get_device_module().get_buffer_device_address(object.vertex_buffer);
	VkDeviceAddress index_address = get_graphics_engine().get_device_module().get_buffer_device_address(object.index_buffer);

	uint32_t maxPrimitiveCount = object.get_num_primitives();

	// Describe buffer as array of VertexObj.
	VkAccelerationStructureGeometryTrianglesDataKHR triangles{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR};
	triangles.vertexFormat             = VK_FORMAT_R32G32B32_SFLOAT;  // vec3 vertex position data.
	triangles.vertexData.deviceAddress = vertex_address;
	triangles.vertexStride             = sizeof(Vertex);
	// Describe index data (32-bit unsigned int)
	triangles.indexType               = VK_INDEX_TYPE_UINT32;
	triangles.indexData.deviceAddress = index_address;
	// Indicate identity transform by setting transformData to null device pointer.
	//triangles.transformData = {};
	triangles.maxVertex = object.get_num_unique_vertices();

	// Identify the above data as containing opaque triangles.
	VkAccelerationStructureGeometryKHR asGeom{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
	asGeom.geometryType       = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
	asGeom.flags              = VK_GEOMETRY_OPAQUE_BIT_KHR;
	asGeom.geometry.triangles = triangles;

	// The entire array will be used to build the BLAS.
	VkAccelerationStructureBuildRangeInfoKHR offset;
	offset.firstVertex     = 0;
	offset.primitiveCount  = maxPrimitiveCount;
	offset.primitiveOffset = 0;
	offset.transformOffset = 0;

	// Our blas is made from only one geometry, but could be made of many geometries
	BlasInput input;
	input.asGeometry.emplace_back(asGeom);
	input.asBuildOffsetInfo.emplace_back(offset);

	return input;
}

template<typename GraphicsEngineT>
void GraphicsEngineRayTracing<GraphicsEngineT>::update_blas()
{
	std::vector<BlasInput> blas_inputs;
	auto& objects = get_graphics_engine().get_objects();
	blas_inputs.reserve(objects.size());
	for (auto& [id, object] : objects)
	{
		blas_inputs.emplace_back(object_to_blas(*object));
	}

	//--------------------------------------------------------------------------------------------------
	// Create all the BLAS from the vector of BlasInput
	// - There will be one BLAS per input-vector entry
	// - There will be as many BLAS as input.size()
	// - The resulting BLAS (along with the inputs used to build) are stored in m_blas,
	//   and can be referenced by index.
	// - if flag has the 'Compact' flag, the BLAS will be compacted
	//
	// m_cmdPool.init(m_device, m_queueIndex);
	auto         nbBlas = static_cast<uint32_t>(blas_inputs.size());
	VkDeviceSize asTotalSize{0};     // Memory size of all allocated BLAS
	uint32_t     nbCompactions{0};   // Nb of BLAS requesting compaction
	VkDeviceSize maxScratchSize{0};  // Largest scratch size

	// Preparing the information for the acceleration build commands.
	std::vector<BuildAccelerationStructure> buildAs(nbBlas);
	for (uint32_t idx = 0; idx < nbBlas; idx++)
	{
		// Filling partially the VkAccelerationStructureBuildGeometryInfoKHR for querying the build sizes.
		// Other information will be filled in the createBlas (see #2)
		buildAs[idx].buildInfo.type          = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
		buildAs[idx].buildInfo.mode          = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
		buildAs[idx].buildInfo.flags         = blas_inputs[idx].flags | VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
		buildAs[idx].buildInfo.geometryCount = static_cast<uint32_t>(blas_inputs[idx].asGeometry.size());
		buildAs[idx].buildInfo.pGeometries   = blas_inputs[idx].asGeometry.data();

		// Build range information
		buildAs[idx].rangeInfo = blas_inputs[idx].asBuildOffsetInfo.data();

		// Finding sizes to create acceleration structures and scratch
		std::vector<uint32_t> maxPrimCount(blas_inputs[idx].asBuildOffsetInfo.size());
		for(auto tt = 0; tt < blas_inputs[idx].asBuildOffsetInfo.size(); tt++)
		{
			maxPrimCount[tt] = blas_inputs[idx].asBuildOffsetInfo[tt].primitiveCount;  // Number of primitives/triangles
		}

		LOAD_VK_FUNCTION(vkGetAccelerationStructureBuildSizesKHR)(
			get_logical_device(), 
			VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
			&buildAs[idx].buildInfo, 
			maxPrimCount.data(), 
			&buildAs[idx].sizeInfo);

		// Extra info
		asTotalSize += buildAs[idx].sizeInfo.accelerationStructureSize;
		maxScratchSize = std::max(maxScratchSize, buildAs[idx].sizeInfo.buildScratchSize);
		if (buildAs[idx].buildInfo.flags & VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR == 
			VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR)
		{
			++nbCompactions;
		}
	}

	// Allocate the scratch buffers holding the temporary data of the acceleration structure builder
	// TODO: track these and ddestroy them
	GraphicsBuffer scratch_buffer = get_graphics_engine().create_buffer(
		maxScratchSize, 
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, 
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	VkDeviceAddress scratchAddress = get_graphics_engine().get_device_module().get_buffer_device_address(scratch_buffer.buffer);

	// Allocate a query pool for storing the needed size for every BLAS compaction.
	VkQueryPool queryPool{VK_NULL_HANDLE};
	if(nbCompactions > 0)  // Is compaction requested?
	{
		assert(nbCompactions == nbBlas);  // Don't allow mix of on/off compaction
		VkQueryPoolCreateInfo qpci{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
		qpci.queryCount = nbBlas;
		qpci.queryType  = VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR;
		vkCreateQueryPool(get_logical_device(), &qpci, nullptr, &queryPool);
	}

	// Batching creation/compaction of BLAS to allow staying in restricted amount of memory
	std::vector<uint32_t> indices;  // Indices of the BLAS to create
	VkDeviceSize          batchSize{0};
	VkDeviceSize          batchLimit{256'000'000};  // 256 MB
	for (uint32_t idx = 0; idx < nbBlas; idx++)
	{
		indices.push_back(idx);
		batchSize += buildAs[idx].sizeInfo.accelerationStructureSize;
		// Over the limit or last BLAS element
		if (batchSize >= batchLimit || idx == nbBlas - 1)
		{
			VkCommandBuffer cmd_buf = get_graphics_engine().begin_single_time_commands();
			cmd_create_blas(cmd_buf, indices, buildAs, scratchAddress, queryPool);
			get_graphics_engine().end_single_time_commands(cmd_buf);

			if (queryPool)
			{
				cmd_buf = get_graphics_engine().begin_single_time_commands();
				cmd_compact_blas(cmd_buf, indices, buildAs, queryPool);
				get_graphics_engine().end_single_time_commands(cmd_buf);

				// Destroy the non-compacted version
				for (auto idx : indices)
				{
					cleanup_acceleration_structure(buildAs[idx].cleanupAS);
				}
			}
			// Reset

			batchSize = 0;
			indices.clear();
		}
	}

	// Logging reduction
	if(queryPool)
	{
		VkDeviceSize compactSize = std::accumulate(buildAs.begin(), buildAs.end(), 0ULL, [](const auto& a, const auto& b) {
			return a + b.sizeInfo.accelerationStructureSize;
		});
		LOG_INFO(Utility::get().get_logger(), "RayTracing: reducing BLAS from: {} to: {}\n", asTotalSize, compactSize);
	}

	// Keeping all the created acceleration structures
	for (auto& b : buildAs)
	{
		bottom_as.emplace_back(b.as);
	}

	// Clean up
	vkDestroyQueryPool(get_logical_device(), queryPool, nullptr);
	vkDestroyBuffer(get_logical_device(), scratch_buffer.buffer, nullptr);
	// TODO: use RAII to destroy device memory as well
}

template<typename GraphicsEngineT>
inline void GraphicsEngineRayTracing<GraphicsEngineT>::update_tlas()
{
	auto& objects = get_graphics_engine().get_objects();
	
	std::vector<VkAccelerationStructureInstanceKHR> tlas;
	tlas.reserve(objects.size());

	uint32_t instance_id = 0; // TODO: this is not correct, need to fix this up properly
	for (auto& [id, object] : objects)
	{
		VkAccelerationStructureInstanceKHR ray_inst{};
		ray_inst.transform = glm_to_vk(object->get_game_object().get_transform());
		ray_inst.instanceCustomIndex = 0; // exists in shader as 'gl_InstanceCustomIndexEXT' TODO: dont hardcode this
		// TOOD: change this to use the actual object id, will need to refactor blas setup
		ray_inst.accelerationStructureReference = getBlasDeviceAddress(instance_id++);
		ray_inst.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
		ray_inst.mask = 0xff; //  Only be hit if rayMask & instance.mask != 0

		// We will use the same hit group for all objects
		// with more shaders maybe this should change
		ray_inst.instanceShaderBindingTableRecordOffset = 0;
		tlas.emplace_back(ray_inst);		
	}

	build_tlas(tlas, VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR, false);
}

template<typename GraphicsEngineT>
void GraphicsEngineRayTracing<GraphicsEngineT>::cmd_create_blas(VkCommandBuffer cmd_buf,
                                                                std::vector<uint32_t> indices,
                                                                std::vector<BuildAccelerationStructure>& build_as,
                                                                VkDeviceAddress scratch_address,
                                                                VkQueryPool query_pool)
{
	if (query_pool)  // For querying the compaction size
	{
		vkResetQueryPool(get_logical_device(), query_pool, 0, static_cast<uint32_t>(indices.size()));
	}

	uint32_t queryCnt{0};

	for(const uint32_t idx : indices)
	{
		// Actual allocation of buffer and acceleration structure.
		VkAccelerationStructureCreateInfoKHR createInfo{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
		createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
		createInfo.size = build_as[idx].sizeInfo.accelerationStructureSize;  // Will be used to allocate memory.
		build_as[idx].as = create_acceleration_structure(createInfo);

		// BuildInfo #2 part
		build_as[idx].buildInfo.dstAccelerationStructure  = build_as[idx].as.accel;  // Setting where the build lands
		build_as[idx].buildInfo.scratchData.deviceAddress = scratch_address;  // All build are using the same scratch buffer

		// Building the bottom-level-acceleration-structure
		LOAD_VK_FUNCTION(vkCmdBuildAccelerationStructuresKHR)(cmd_buf, 1, &build_as[idx].buildInfo, &build_as[idx].rangeInfo);

		// Since the scratch buffer is reused across builds, we need a barrier to ensure one build
		// is finished before starting the next one.
		VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
		barrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
		barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
		vkCmdPipelineBarrier(
			cmd_buf, 
			VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
			VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 
			0, 
			1, 
			&barrier, 
			0, 
			nullptr, 
			0, 
			nullptr);

		if(query_pool)
		{
			// Add a query to find the 'real' amount of memory needed, use for compaction
			LOAD_VK_FUNCTION(vkCmdWriteAccelerationStructuresPropertiesKHR)(
				cmd_buf, 
				1, 
				&build_as[idx].buildInfo.dstAccelerationStructure,
				VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR, 
				query_pool, 
				queryCnt++);
		}
	}
}

template<typename GraphicsEngineT>
void GraphicsEngineRayTracing<GraphicsEngineT>::cmd_compact_blas(VkCommandBuffer cmd_buf,
                                                                 std::vector<uint32_t> indices,
                                                                 std::vector<BuildAccelerationStructure>& build_as,
                                                                 VkQueryPool query_pool)
{
	// Get the compacted size result back
	std::vector<VkDeviceSize> compactSizes(static_cast<uint32_t>(indices.size()));
	vkGetQueryPoolResults(get_logical_device(), 
						  query_pool, 
						  0, 
						  (uint32_t)compactSizes.size(), 
						  compactSizes.size() * sizeof(VkDeviceSize),
						  compactSizes.data(), 
						  sizeof(VkDeviceSize), 
						  VK_QUERY_RESULT_WAIT_BIT);

	uint32_t queryCtn{0};
	for(const uint32_t idx : indices)
	{
		build_as[idx].cleanupAS                          = build_as[idx].as;           // previous AS to destroy
		build_as[idx].sizeInfo.accelerationStructureSize = compactSizes[queryCtn++];  // new reduced size
		
		// Creating a compact version of the AS
		VkAccelerationStructureCreateInfoKHR asCreateInfo{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
		asCreateInfo.size = build_as[idx].sizeInfo.accelerationStructureSize;
		asCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;

		build_as[idx].as = create_acceleration_structure(asCreateInfo);

		// Copy the original BLAS to a compact version
		VkCopyAccelerationStructureInfoKHR copyInfo{VK_STRUCTURE_TYPE_COPY_ACCELERATION_STRUCTURE_INFO_KHR};
		copyInfo.src  = build_as[idx].buildInfo.dstAccelerationStructure;
		copyInfo.dst  = build_as[idx].as.accel;
		copyInfo.mode = VK_COPY_ACCELERATION_STRUCTURE_MODE_COMPACT_KHR;
		LOAD_VK_FUNCTION(vkCmdCopyAccelerationStructureKHR)(cmd_buf, &copyInfo);
	}	
}

template<typename GraphicsEngineT>
void GraphicsEngineRayTracing<GraphicsEngineT>::cmd_create_tlas(
	VkCommandBuffer cmd_buf,
	uint32_t nInstances,
	VkDeviceAddress inst_buffer_addr,
	GraphicsBuffer& scratch_buffer,
	VkBuildAccelerationStructureFlagsKHR flags,
	bool update)
{
	// Wraps a device pointer to the above uploaded instances.
	VkAccelerationStructureGeometryInstancesDataKHR instancesVk{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR};
	instancesVk.data.deviceAddress = inst_buffer_addr;

	// Put the above into a VkAccelerationStructureGeometryKHR. We need to put the instances struct in a union and label it as instance data.
	VkAccelerationStructureGeometryKHR topASGeometry{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
	topASGeometry.geometryType       = VK_GEOMETRY_TYPE_INSTANCES_KHR;
	topASGeometry.geometry.instances = instancesVk;

	// Find sizes
	VkAccelerationStructureBuildGeometryInfoKHR buildInfo{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR};
	buildInfo.flags         = flags;
	buildInfo.geometryCount = 1;
	buildInfo.pGeometries   = &topASGeometry;
	buildInfo.mode = update ? VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR : VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	buildInfo.type                     = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
	buildInfo.srcAccelerationStructure = VK_NULL_HANDLE;

	VkAccelerationStructureBuildSizesInfoKHR sizeInfo{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
	LOAD_VK_FUNCTION(vkGetAccelerationStructureBuildSizesKHR)(
		get_logical_device(), 
		VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, 
		&buildInfo,
		&nInstances, 
		&sizeInfo);

    VkAccelerationStructureCreateInfoKHR createInfo{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
    createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    createInfo.size = sizeInfo.accelerationStructureSize;

    top_as = create_acceleration_structure(createInfo);
	
	// Allocate the scratch memory
	scratch_buffer = get_graphics_engine().create_buffer(
		sizeInfo.buildScratchSize, 
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, 
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	VkDeviceAddress scratch_address = get_graphics_engine().get_device_module().get_buffer_device_address(scratch_buffer.buffer);

	// Finally build the acceleration structure

	// Update build information
	buildInfo.srcAccelerationStructure  = VK_NULL_HANDLE;
	buildInfo.dstAccelerationStructure  = top_as.accel;
	buildInfo.scratchData.deviceAddress = scratch_address;

	// Build Offsets info: n instances
	VkAccelerationStructureBuildRangeInfoKHR        buildOffsetInfo{nInstances, 0, 0, 0};
	const VkAccelerationStructureBuildRangeInfoKHR* pBuildOffsetInfo = &buildOffsetInfo;

	// Build the TLAS
	LOAD_VK_FUNCTION(vkCmdBuildAccelerationStructuresKHR)(cmd_buf, 1, &buildInfo, &pBuildOffsetInfo);
}

template<typename GraphicsEngineT>
typename GraphicsEngineRayTracing<GraphicsEngineT>::AccelerationStructure 
	GraphicsEngineRayTracing<GraphicsEngineT>::create_acceleration_structure(
	VkAccelerationStructureCreateInfoKHR& create_info)
{
	// Allocating the buffer to hold the acceleration structure
	// TODO: might need to clean the buffer and device memory up
	AccelerationStructure resultAccel;
	create_buffer(
		create_info.size, 
		VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		resultAccel.buffer,
		resultAccel.memory);
	create_info.buffer = resultAccel.buffer;

	LOAD_VK_FUNCTION(vkCreateAccelerationStructureKHR)(get_logical_device(), &create_info, nullptr, &resultAccel.accel);

	return resultAccel;
}

template<typename GraphicsEngineT>
inline VkDeviceAddress GraphicsEngineRayTracing<GraphicsEngineT>::getBlasDeviceAddress(uint32_t blasId)
{
	assert(size_t(blasId) < bottom_as.size());
	VkAccelerationStructureDeviceAddressInfoKHR addressInfo{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR};
	addressInfo.accelerationStructure = bottom_as[blasId].accel;

	return LOAD_VK_FUNCTION(vkGetAccelerationStructureDeviceAddressKHR)(get_logical_device(), &addressInfo);
}

template<typename GraphicsEngineT>
inline void GraphicsEngineRayTracing<GraphicsEngineT>::build_tlas(
	const std::vector<VkAccelerationStructureInstanceKHR>& instances,
	VkBuildAccelerationStructureFlagsKHR flags,
	bool update)
{
	if (top_as.accel != VK_NULL_HANDLE && !update)
	{
		throw std::runtime_error("Cannot call buildTlas twice except to update.");
	}

	const auto instance_buffer_usage_flags = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
		VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
    // Create a buffer holding the actual instance data (matrices++) for use by the AS builder
	GraphicsBuffer instance_buffer = get_graphics_engine().create_buffer_from_data(
		instances.data(),
		instances.size() * sizeof(VkAccelerationStructureInstanceKHR),
		instance_buffer_usage_flags,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	
	VkCommandBuffer cmd_buf = get_graphics_engine().begin_single_time_commands();
	GraphicsBuffer scratch_buffer;
	cmd_create_tlas(
		cmd_buf, 
		instances.size(), 
		get_graphics_engine().get_device_module().get_buffer_device_address(instance_buffer.buffer),
		scratch_buffer,
		flags, 
		update);
	get_graphics_engine().end_single_time_commands(cmd_buf);

	// TODO: cleanup instance_buffer
	// also scratch_buffer
}
