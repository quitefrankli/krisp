#pragma once

#include "graphics_engine_base_module.hpp"

#include <queue>


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

	// i.e. sphere uses 1 uniform while cube uses 6 per descriptor set
	const int MAX_UNIFORMS_PER_DESCRIPTOR_SET = 10;
	const int MAX_COMBINED_IMAGE_SAMPLERS_PER_DESCRIPTOR_SET = 10;
	const int MAX_IMGUI_DESCRIPTOR_SETS = 50;

	std::vector<VkDescriptorSet> reserve_descriptor_sets(int n);
	void free_descriptor_sets(std::vector<VkDescriptorSet>& sets);

	int get_max_descriptor_sets() const;

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

public:
	// a descriptor set layout describes the layout for a specific "descriptor set"
	VkDescriptorSetLayout& low_freq_descriptor_set_layout;
	VkDescriptorSetLayout& high_freq_descriptor_set_layout;
	std::array<VkDescriptorSetLayout, 2> descriptor_set_layouts;
	VkDescriptorSet global_descriptor_set;
};