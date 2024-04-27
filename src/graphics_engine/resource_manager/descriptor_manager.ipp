#include "descriptor_manager.hpp"
#include "graphics_buffer_manager.hpp"


static constexpr VkDescriptorSetLayoutBinding get_generic_global_binding()
{
	// global uniforms i.e. camera & lighting
	VkDescriptorSetLayoutBinding gubo_layout_binding{};
	gubo_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	gubo_layout_binding.binding = SDS::GLOBAL_DATA_BINDING;
	gubo_layout_binding.descriptorCount = 1;
	// defines which shader stage the descriptor is going to be referenced
	gubo_layout_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | 
		VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
	gubo_layout_binding.pImmutableSamplers = nullptr; // only relevant for image sampling related descriptors

	return gubo_layout_binding;
}

static constexpr VkDescriptorSetLayoutBinding get_generic_obj_ubo_binding()
{
	VkDescriptorSetLayoutBinding ubo_layout_binding{};
	ubo_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	ubo_layout_binding.binding = SDS::RASTERIZATION_OBJECT_DATA_BINDING;
	ubo_layout_binding.descriptorCount = 1;
	ubo_layout_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT; // defines which shader stage the descriptor is going to be referenced
	ubo_layout_binding.pImmutableSamplers = nullptr; // only relevant for image sampling related descriptors

	return ubo_layout_binding;
}

static constexpr VkDescriptorSetLayoutBinding get_generic_material_binding()
{
	VkDescriptorSetLayoutBinding materials_layout_binding{};
	materials_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	materials_layout_binding.binding = SDS::RASTERIZATION_MATERIAL_DATA_BINDING;
	materials_layout_binding.descriptorCount = 1;
	materials_layout_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	materials_layout_binding.pImmutableSamplers = nullptr;

	return materials_layout_binding;
}

static constexpr VkDescriptorSetLayoutBinding get_generic_texture_binding()
{
	VkDescriptorSetLayoutBinding sampler_layout_binding{};
	sampler_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	sampler_layout_binding.binding = SDS::RASTERIZATION_ALBEDO_TEXTURE_DATA_BINDING;
	sampler_layout_binding.descriptorCount = 1;
	sampler_layout_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT; // defines which shader stage the descriptor is going to be referenced
	sampler_layout_binding.pImmutableSamplers = nullptr; // only relevant for image sampling related descriptors

	return sampler_layout_binding;
}

static constexpr VkDescriptorSetLayoutBinding get_generic_bone_binding()
{
	// buffer of bone data containing transformation matrices
	VkDescriptorSetLayoutBinding bone_layout_binding{};
	bone_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	bone_layout_binding.binding = SDS::RASTERIZATION_BONE_DATA_BINDING;
	bone_layout_binding.descriptorCount = 1;
	bone_layout_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	bone_layout_binding.pImmutableSamplers = nullptr;

	return bone_layout_binding;
}

static constexpr VkDescriptorSetLayoutBinding get_generic_raytracing_tlas_binding()
{
	VkDescriptorSetLayoutBinding tlas_layout_binding{};
	tlas_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
	tlas_layout_binding.binding = SDS::RAYTRACING_TLAS_DATA_BINDING;
	tlas_layout_binding.descriptorCount = 1;
	tlas_layout_binding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
	tlas_layout_binding.pImmutableSamplers = nullptr;

	return tlas_layout_binding;
}

static constexpr VkDescriptorSetLayoutBinding get_generic_raytracing_output_image_binding()
{
	VkDescriptorSetLayoutBinding output_layout_binding{};
	output_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	output_layout_binding.binding = SDS::RAYTRACING_OUTPUT_IMAGE_BINDING;
	output_layout_binding.descriptorCount = 1;
	output_layout_binding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
	output_layout_binding.pImmutableSamplers = nullptr;

	return output_layout_binding;
}

static constexpr VkDescriptorSetLayoutBinding get_generic_mesh_data_buffer_map_binding()
{
	VkDescriptorSetLayoutBinding buffer_mapper_binding{};
	buffer_mapper_binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	buffer_mapper_binding.binding = SDS::RAYTRACING_BUFFER_MAPPER_BINDING;
	buffer_mapper_binding.descriptorCount = 1;
	buffer_mapper_binding.stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
	buffer_mapper_binding.pImmutableSamplers = nullptr;

	return buffer_mapper_binding;
}

static constexpr VkDescriptorSetLayoutBinding get_generic_mesh_data_vertices_binding()
{
	VkDescriptorSetLayoutBinding vertices_binding{};
	vertices_binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	vertices_binding.binding = SDS::RAYTRACING_VERTICES_DATA_BINDING;
	vertices_binding.descriptorCount = 1;
	vertices_binding.stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
	vertices_binding.pImmutableSamplers = nullptr;

	return vertices_binding;
}

static constexpr VkDescriptorSetLayoutBinding get_generic_mesh_data_indices_binding()
{
	VkDescriptorSetLayoutBinding indices_binding{};
	indices_binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	indices_binding.binding = SDS::RAYTRACING_INDICES_DATA_BINDING;
	indices_binding.descriptorCount = 1;
	indices_binding.stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
	indices_binding.pImmutableSamplers = nullptr;

	return indices_binding;
}

static constexpr VkDescriptorSetLayoutBinding get_generic_shadow_map_binding()
{
	VkDescriptorSetLayoutBinding shadow_map_binding{};
	shadow_map_binding.binding = SDS::RASTERIZATION_SHADOW_MAP_DATA_BINDING;
	shadow_map_binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	shadow_map_binding.descriptorCount = 1;
	shadow_map_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	shadow_map_binding.pImmutableSamplers = nullptr; // Optional

	return shadow_map_binding;
}

GraphicsDescriptorManager::GraphicsDescriptorManager(
	GraphicsEngine& engine,
	const GraphicsBufferManager& buffer_manager) :
	GraphicsEngineBaseModule(engine)
{
	create_descriptor_pool();
	setup_descriptor_set_layouts();
	const auto get_gubo_offsets = [&buffer_manager] {
		std::vector<uint32_t> offsets;
		for (uint32_t frame_idx = 0; frame_idx < GraphicsEngine::get_num_swapchain_images(); ++frame_idx)
		{
			offsets.push_back(buffer_manager.get_global_uniform_buffer_offset(frame_idx));
		}
		return offsets;
	};
	allocate_global_dset(buffer_manager.get_global_uniform_buffer(), get_gubo_offsets());
	allocate_mesh_data_dset(
		buffer_manager.get_mapping_buffer(), 
		buffer_manager.get_vertex_buffer(),
		buffer_manager.get_index_buffer());
}

GraphicsDescriptorManager::~GraphicsDescriptorManager()
{
	vkDestroyDescriptorPool(get_logical_device(), descriptor_pool, nullptr);
	for (auto layout : all_dset_layouts)
	{
		vkDestroyDescriptorSetLayout(get_logical_device(), layout, nullptr);
	}
}

VkDescriptorSetLayout GraphicsDescriptorManager::request_dset_layout(const std::vector<VkDescriptorSetLayoutBinding>& bindings)
{
	VkDescriptorSetLayoutCreateInfo layout_info{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
	layout_info.bindingCount = bindings.size();
	layout_info.pBindings = bindings.data();

	VkDescriptorSetLayout layout;
	if (vkCreateDescriptorSetLayout(get_logical_device(), &layout_info, nullptr, &layout) != VK_SUCCESS)
	{
		throw std::runtime_error("GraphicsDescriptorManager: failed to create descriptor set layout!");
	}

	all_dset_layouts.push_back(layout);

	return layout;
}

VkDescriptorSet GraphicsDescriptorManager::reserve_dset(const VkDescriptorSetLayout layout)
{
	return reserve_dsets({ layout })[0];
}

std::vector<VkDescriptorSet> GraphicsDescriptorManager::reserve_dsets(const std::vector<VkDescriptorSetLayout>& layouts)
{
	VkDescriptorSetAllocateInfo alloc_info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
	alloc_info.descriptorPool = descriptor_pool;
	alloc_info.descriptorSetCount = layouts.size();
	alloc_info.pSetLayouts = layouts.data();
	std::vector<VkDescriptorSet> descriptor_sets(alloc_info.descriptorSetCount);
	if (vkAllocateDescriptorSets(get_logical_device(), &alloc_info, descriptor_sets.data()) != VK_SUCCESS)
	{
		throw std::runtime_error("GraphicsDescriptorManager: failed to allocate high freq descriptor sets!");
	}

	return descriptor_sets;
}

void GraphicsDescriptorManager::free_dset(VkDescriptorSet set)
{
	std::vector<VkDescriptorSet> dsets{ set };
	free_dsets(dsets);
}

void GraphicsDescriptorManager::free_dsets(std::vector<VkDescriptorSet>& dsets)
{
	if (dsets.empty())
	{
		return;
	}

	if (vkFreeDescriptorSets(
		get_graphics_engine().get_logical_device(), 
		descriptor_pool, 
		dsets.size(), 
		dsets.data()) != VK_SUCCESS)
	{
		throw std::runtime_error("GraphicsDescriptorManager: failed to free descriptor sets!");
	}
}

void GraphicsDescriptorManager::create_descriptor_pool()
{
	VkDescriptorPoolSize uniform_buffer_pool_size{};
	uniform_buffer_pool_size.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	// max number of uniform buffers per descriptor set
	uniform_buffer_pool_size.descriptorCount = MAX_UNIFORMS_PER_DESCRIPTOR_SET * MAX_HIGH_FREQ_DESCRIPTOR_SETS;

	VkDescriptorPoolSize combined_image_sampler_pool_size{};
	combined_image_sampler_pool_size.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	// max number of combined image samplers per descriptor set
	combined_image_sampler_pool_size.descriptorCount = MAX_COMBINED_IMAGE_SAMPLERS_PER_DESCRIPTOR_SET * MAX_HIGH_FREQ_DESCRIPTOR_SETS;

	// for ray tracing
	VkDescriptorPoolSize tlas_pool_size{};
	tlas_pool_size.type = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
	tlas_pool_size.descriptorCount = MAX_RAY_TRACING_DESCRIPTOR_SETS;

	VkDescriptorPoolSize rt_storage_image_pool_size{};
	rt_storage_image_pool_size.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	rt_storage_image_pool_size.descriptorCount = MAX_RAY_TRACING_DESCRIPTOR_SETS;

	// for meshes, materials and bones
	VkDescriptorPoolSize storage_buffer_pool_size{};
	storage_buffer_pool_size.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	storage_buffer_pool_size.descriptorCount = MAX_STORAGE_BUFFER_DESCRIPTOR_SETS;

	std::vector<VkDescriptorPoolSize> pool_sizes {
		uniform_buffer_pool_size, 
		combined_image_sampler_pool_size,
		tlas_pool_size,
		rt_storage_image_pool_size,
		storage_buffer_pool_size
	};

	VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
	poolInfo.poolSizeCount = pool_sizes.size();
	poolInfo.pPoolSizes = pool_sizes.data();
	// defines maximum number of descriptor sets that may be allocated
	poolInfo.maxSets = MAX_DESCRIPTOR_SETS;
	// This flag specifies that descriptor sets can be freed individually
	// Also according to https://vkguide.dev/docs/chapter-4/descriptors/ having this flag
	//	enabled means descriptor set allocations from the pool are very cheap
	poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

	LOG_INFO(Utility::get().get_logger(), 
			 "GraphicsDescriptorManager::create_descriptor_pool: max_sets:={}\n",
		   	 poolInfo.maxSets);

	if (vkCreateDescriptorPool(get_logical_device(), &poolInfo, nullptr, &descriptor_pool) != VK_SUCCESS)
	{
		throw std::runtime_error("GraphicsEngine::create_descriptor_pool: failed to create descriptor pool!");
	}
}

std::vector<VkDescriptorSetLayout> GraphicsDescriptorManager::
	get_rasterization_descriptor_set_layouts() const
{
	return { 
		low_freq_dset_layout,
		per_obj_dset_layout,
		renderable_dset_layout, 
		shadow_map_dset_layout
	};
}

std::vector<VkDescriptorSetLayout> GraphicsDescriptorManager::
	get_raytracing_descriptor_set_layouts() const
{
	return { 
		low_freq_dset_layout,
		raytracing_tlas_dset_layout, 
		mesh_data_dset_layout
	};
}

void GraphicsDescriptorManager::setup_descriptor_set_layouts()
{
	low_freq_dset_layout = request_dset_layout({ get_generic_global_binding() });
	per_obj_dset_layout = request_dset_layout({ get_generic_obj_ubo_binding(), get_generic_bone_binding() });
	renderable_dset_layout = request_dset_layout({ 
		get_generic_material_binding(), 
		get_generic_texture_binding() });
	shadow_map_dset_layout = request_dset_layout({ get_generic_shadow_map_binding() });
	mesh_data_dset_layout = request_dset_layout({ 
		get_generic_mesh_data_buffer_map_binding(),
		get_generic_mesh_data_vertices_binding(),
		get_generic_mesh_data_indices_binding() });
	raytracing_tlas_dset_layout = request_dset_layout({
		get_generic_raytracing_tlas_binding(),
		get_generic_raytracing_output_image_binding() });
}

void GraphicsDescriptorManager::allocate_global_dset(VkBuffer global_buffer, const std::vector<uint32_t>& global_buffer_offsets)
{
	assert(global_buffer_offsets.size() == MAX_LOW_FREQ_DESCRIPTOR_SETS);

	std::vector<VkDescriptorSetLayout> dset_layouts(MAX_LOW_FREQ_DESCRIPTOR_SETS, low_freq_dset_layout);
	VkDescriptorSetAllocateInfo alloc_info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
	alloc_info.descriptorPool = descriptor_pool;
	alloc_info.descriptorSetCount = dset_layouts.size();
	alloc_info.pSetLayouts = dset_layouts.data();

	global_dsets.resize(MAX_LOW_FREQ_DESCRIPTOR_SETS);
	if (vkAllocateDescriptorSets(
		get_logical_device(), 
		&alloc_info, 
		global_dsets.data()) != VK_SUCCESS)
	{
		throw std::runtime_error("GraphicsResourceManager: failed to allocate low freq descriptor sets!");
	}

	for (int i = 0; i < global_dsets.size(); ++i)
	{
		VkDescriptorBufferInfo buffer_info{};
		buffer_info.buffer = global_buffer;
		buffer_info.offset = global_buffer_offsets[i];
		buffer_info.range = sizeof(SDS::GlobalData);

		VkWriteDescriptorSet dset_write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
		dset_write.dstSet = global_dsets[i];
		dset_write.dstBinding = SDS::GLOBAL_DATA_BINDING;
		dset_write.dstArrayElement = 0;
		dset_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		dset_write.descriptorCount = 1;
		dset_write.pBufferInfo = &buffer_info;
		dset_write.pImageInfo = nullptr;
		dset_write.pTexelBufferView = nullptr;

		vkUpdateDescriptorSets(
			get_logical_device(), 
			1, 
			&dset_write, 
			0, 
			nullptr);
	}
}

void GraphicsDescriptorManager::allocate_mesh_data_dset(
	VkBuffer mapping_buffer, VkBuffer vertex_buffer, VkBuffer index_buffer)
{
	std::vector<VkDescriptorSetLayout> layouts(MAX_MESH_DATA_DESCRIPTOR_SETS, mesh_data_dset_layout);
	VkDescriptorSetAllocateInfo alloc_info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
	alloc_info.descriptorPool = descriptor_pool;
	alloc_info.descriptorSetCount = 1;
	alloc_info.pSetLayouts = layouts.data();
	if (vkAllocateDescriptorSets(get_logical_device(), &alloc_info, &mesh_data_dset) != VK_SUCCESS)
	{
		throw std::runtime_error("GraphicsResourceManager: failed to allocate mesh data descriptor set!");
	}

	// create descriptor set for object's mesh and material (not implemented yet) data
	using buffer_mgr_t = GraphicsBufferManager;
	VkDescriptorBufferInfo buffer_mapper_info{};
	buffer_mapper_info.buffer = mapping_buffer;
	buffer_mapper_info.offset = 0;
	buffer_mapper_info.range = buffer_mgr_t::MAPPING_BUFFER_CAPACITY;
	VkWriteDescriptorSet buffer_mapper_dset_write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
	buffer_mapper_dset_write.dstSet = mesh_data_dset;
	buffer_mapper_dset_write.dstBinding = SDS::BUFFER_MAPPER_BINDING;
	buffer_mapper_dset_write.dstArrayElement = 0;
	buffer_mapper_dset_write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	buffer_mapper_dset_write.descriptorCount = 1;
	buffer_mapper_dset_write.pBufferInfo = &buffer_mapper_info;

	VkDescriptorBufferInfo vertices_buffer_info{};
	vertices_buffer_info.buffer = vertex_buffer;
	vertices_buffer_info.offset = 0;
	vertices_buffer_info.range = buffer_mgr_t::VERTEX_BUFFER_CAPACITY;
	VkWriteDescriptorSet vertices_dset_write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
	vertices_dset_write.dstSet = mesh_data_dset;
	vertices_dset_write.dstBinding = SDS::VERTICES_DATA_BINDING;
	vertices_dset_write.dstArrayElement = 0;
	vertices_dset_write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	vertices_dset_write.descriptorCount = 1;
	vertices_dset_write.pBufferInfo = &vertices_buffer_info;

	VkDescriptorBufferInfo indices_buffer_info{};
	indices_buffer_info.buffer = index_buffer;
	indices_buffer_info.offset = 0;
	indices_buffer_info.range = buffer_mgr_t::INDEX_BUFFER_CAPACITY;
	VkWriteDescriptorSet indices_dset_write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
	indices_dset_write.dstSet = mesh_data_dset;
	indices_dset_write.dstBinding = SDS::INDICES_DATA_BINDING;
	indices_dset_write.dstArrayElement = 0;
	indices_dset_write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	indices_dset_write.descriptorCount = 1;
	indices_dset_write.pBufferInfo = &indices_buffer_info;

	std::vector<VkWriteDescriptorSet> dsets{buffer_mapper_dset_write, vertices_dset_write, indices_dset_write};

	vkUpdateDescriptorSets(
		get_logical_device(),
		static_cast<uint32_t>(dsets.size()),
		dsets.data(),
		0,
		nullptr);
}
