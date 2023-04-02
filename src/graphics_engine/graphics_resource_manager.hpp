#pragma once

#include "graphics_engine_base_module.hpp"

#include <queue>


struct RaytracingResources
{
	VkDescriptorSet ray_tracing_descriptor_set;
	VkDescriptorSetLayoutBinding tlas_binding;
	VkDescriptorSetLayoutBinding output_binding;

	std::vector<VkDescriptorSetLayoutBinding> get_layout_bindings()
	{
		return { tlas_binding, output_binding };
	}
};

template<typename GraphicsEngineT>
class GraphicsResourceManager : public GraphicsEngineBaseModule<GraphicsEngineT>
{
public:
	GraphicsResourceManager(GraphicsEngineT& engine);
	~GraphicsResourceManager();
	VkCommandPool& get_command_pool() { return command_pool; }
	VkDescriptorPool descriptor_pool;

	VkBuffer global_uniform_buffer;
	VkDeviceMemory global_uniform_buffer_memory;

	void allocate_descriptor_set();
	void allocate_descriptor_sets_for_raytracing(VkImageView rt_image_view);

	// i.e. sphere uses 1 uniform while cube uses 6 per descriptor set
	static constexpr int MAX_UNIFORMS_PER_DESCRIPTOR_SET = 10;
	static constexpr int MAX_COMBINED_IMAGE_SAMPLERS_PER_DESCRIPTOR_SET = 10;
	static constexpr int MAX_IMGUI_DESCRIPTOR_SETS = 50;

	std::vector<VkDescriptorSet> reserve_descriptor_sets(int n);
	void free_descriptor_sets(std::vector<VkDescriptorSet>& sets);

	static constexpr int get_max_descriptor_sets();

private:
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_graphics_engine;
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_logical_device;
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_physical_device;
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_instance;
	using GraphicsEngineBaseModule<GraphicsEngineT>::create_buffer;
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_num_swapchain_frames;
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_render_pass;
	
	void create_command_pool();
	void create_descriptor_pool();
	void create_descriptor_set_layout();
	VkCommandPool command_pool;

	std::queue<VkDescriptorSet> available_descriptor_sets;
	RaytracingResources ray_tracing_resources;

	static constexpr int MAX_LOW_FREQ_DESCRIPTOR_SETS = 1; // for GUBO i.e. camera & lighting
	static constexpr int MAX_HIGH_FREQ_DESCRIPTOR_SETS = 1000; // for objects i.e. model + texture
	static constexpr int MAX_RAY_TRACING_DESCRIPTOR_SETS = 1; // for ray tracing

public:
	std::array<VkDescriptorSetLayout, 3> descriptor_set_layouts;
	// a descriptor set layout describes the layout for a specific "descriptor set"
	VkDescriptorSetLayout& low_freq_descriptor_set_layout;
	VkDescriptorSetLayout& high_freq_descriptor_set_layout;
	VkDescriptorSetLayout& ray_tracing_descriptor_set_layout;
	VkDescriptorSet global_descriptor_set;
};