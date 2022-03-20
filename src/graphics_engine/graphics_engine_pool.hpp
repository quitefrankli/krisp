#pragma once

#include "graphics_engine_base_module.hpp"

#include <array>


class GraphicsEnginePool : public GraphicsEngineBaseModule
{
public:
	GraphicsEnginePool(GraphicsEngine& engine);
	~GraphicsEnginePool();
	VkCommandPool& get_command_pool() { return command_pool; }
	VkDescriptorPool descriptor_pool;

	VkBuffer global_uniform_buffer;
	VkDeviceMemory global_uniform_buffer_memory;

	void allocate_descriptor_set();

	const int MAX_UNIFORMS_PER_DESCRIPTOR_SET = 10;
	const int MAX_COMBINED_IMAGE_SAMPLERS_PER_DESCRIPTOR_SET = 10;

private:
	void create_command_pool();
	void create_descriptor_pool();
	void create_descriptor_set_layout();
	VkCommandPool command_pool;


public:
	// a descriptor set layout describes the layout for a specific "descriptor set"
	VkDescriptorSetLayout& low_freq_descriptor_set_layout;
	VkDescriptorSetLayout& high_freq_descriptor_set_layout;
	std::array<VkDescriptorSetLayout, 2> descriptor_set_layouts;
	VkDescriptorSet global_descriptor_set;
};