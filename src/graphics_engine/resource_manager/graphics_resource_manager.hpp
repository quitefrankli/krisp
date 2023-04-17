#pragma once

#include "graphics_buffer_manager.hpp"
#include "graphics_engine/graphics_engine_swap_chain.hpp"

#include <queue>


struct RaytracingResources
{
	VkDescriptorSetLayout descriptor_set_layout;
	VkDescriptorSetLayoutBinding tlas_binding;
	VkDescriptorSetLayoutBinding output_binding;

	std::vector<VkDescriptorSetLayoutBinding> get_layout_bindings()
	{
		return { tlas_binding, output_binding };
	}
};

template<typename GraphicsEngineT>
class GraphicsResourceManager : public GraphicsBufferManager<GraphicsEngineT>
{
public:
	GraphicsResourceManager(GraphicsEngineT& engine);
	virtual ~GraphicsResourceManager() override;
	VkCommandPool& get_command_pool() { return command_pool; }
	VkDescriptorPool descriptor_pool;

	VkCommandBuffer create_command_buffer();

	RaytracingResources& get_raytracing_resources() { return ray_tracing_resources; }

	// other graphics components should call these methods to request descriptor sets
	std::vector<VkDescriptorSet> reserve_high_frequency_dsets(uint32_t n)
	{
		return reserve_dsets_common_impl(available_high_freq_dsets, n);
	}
	std::vector<VkDescriptorSet> reserve_raytracing_dsets(uint32_t n)
	{
		return reserve_dsets_common_impl(available_raytracing_dsets, n);
	}

	// other graphics components should call these methods to release dsets back into the pool
	void free_high_frequency_dsets(std::vector<VkDescriptorSet>& sets)
	{
		free_dsets_common_impl(available_high_freq_dsets, sets);
	}
	void free_raytracing_dsets(std::vector<VkDescriptorSet>& sets)
	{
		free_dsets_common_impl(available_raytracing_dsets, sets);
	}

	// used in binding descriptor sets during draw
	// corresponds to the "set" value in the "layout" in shaders
	struct {
		static constexpr int RASTERIZATION_LOW_FREQ = 1;
		static constexpr int RASTERIZATION_HIGH_FREQ = 0;
		static constexpr int RAYTRACING_LOW_FREQ = 1;
		static constexpr int RAYTRACING_TLAS = 0;
	} DSET_OFFSETS;

	std::vector<VkDescriptorSetLayout> get_rasterization_descriptor_set_layouts() const;
	std::vector<VkDescriptorSetLayout> get_raytracing_descriptor_set_layouts() const;

	const VkDescriptorSet& get_low_freq_dset() const { return global_descriptor_set; }
	VkDescriptorSet get_mesh_data_dset() const { return mesh_data_descriptor_set; }

private:
	// using GraphicsEngineBaseModule<GraphicsEngineT>::get_graphics_engine;
	// using GraphicsEngineBaseModule<GraphicsEngineT>::get_logical_device;
	// using GraphicsEngineBaseModule<GraphicsEngineT>::get_physical_device;
	// using GraphicsEngineBaseModule<GraphicsEngineT>::get_instance;
	// using GraphicsEngineBaseModule<GraphicsEngineT>::create_buffer;
	// using GraphicsEngineBaseModule<GraphicsEngineT>::get_num_swapchain_frames;
		
	void allocate_rasterization_dsets();
	void allocate_raytracing_dsets();
	void allocate_mesh_data_dsets();
	void initialise_global_descriptor_set(); // aka low_frequency
	void initialise_mesh_data_descriptor_set();

	void create_command_pool();
	void create_descriptor_pool();
	void create_descriptor_set_layouts();
	std::vector<VkDescriptorSetLayout> get_all_descriptor_set_layouts();
	std::vector<VkDescriptorSet> reserve_dsets_common_impl(
		std::queue<VkDescriptorSet>& queue, uint32_t n);
	void free_dsets_common_impl(
		std::queue<VkDescriptorSet>& queue, std::vector<VkDescriptorSet>& sets);

	VkCommandPool command_pool;
	VkDescriptorSet global_descriptor_set;
	VkDescriptorSet mesh_data_descriptor_set;
	std::queue<VkDescriptorSet> available_high_freq_dsets;
	std::queue<VkDescriptorSet> available_raytracing_dsets;
	RaytracingResources ray_tracing_resources;

	static constexpr int MAX_LOW_FREQ_DESCRIPTOR_SETS = 1; // for GUBO i.e. camera & lighting
	static constexpr int MAX_HIGH_FREQ_DESCRIPTOR_SETS = 1000; // for objects i.e. model + texture
	static constexpr int MAX_RAY_TRACING_DESCRIPTOR_SETS = 1000; // for ray tracing
	static constexpr int MAX_MESH_DATA_DESCRIPTOR_SETS = 1;
	static constexpr int MAX_STORAGE_BUFFER_DESCRIPTOR_SETS = 10;

	// i.e. sphere uses 1 uniform while cube uses 6 per descriptor set
	static constexpr int MAX_UNIFORMS_PER_DESCRIPTOR_SET = 10;
	static constexpr int MAX_COMBINED_IMAGE_SAMPLERS_PER_DESCRIPTOR_SET = 10;
	static constexpr int MAX_IMGUI_DESCRIPTOR_SETS = 50;

	// upper bound for descriptor sets, statically checked
	static constexpr int MAX_DESCRIPTOR_SETS = 5000;

	static_assert(
		MAX_LOW_FREQ_DESCRIPTOR_SETS + MAX_HIGH_FREQ_DESCRIPTOR_SETS + 
		MAX_RAY_TRACING_DESCRIPTOR_SETS + MAX_MESH_DATA_DESCRIPTOR_SETS +
		MAX_IMGUI_DESCRIPTOR_SETS <= MAX_DESCRIPTOR_SETS, 
		"GraphicsResourceManager: too many descriptor sets!");

	// a descriptor set layout describes the layout for a specific "descriptor set"
	VkDescriptorSetLayout low_freq_descriptor_set_layout;
	VkDescriptorSetLayout high_freq_descriptor_set_layout;
	VkDescriptorSetLayout mesh_data_descriptor_set_layout;
};