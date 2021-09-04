#include "graphics_engine.hpp"

void GraphicsEngine::create_descriptor_set_layout()
{
	VkDescriptorSetLayoutBinding ubo_layout_binding{};
	ubo_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	ubo_layout_binding.binding = 0; // this must be synced with the one in the shaders
	ubo_layout_binding.descriptorCount = 1;
	ubo_layout_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT; // defines which shader stage the descriptor is going to be referenced
	ubo_layout_binding.pImmutableSamplers = nullptr; // only relevant for image sampling related descriptors

	VkDescriptorSetLayoutBinding sampler_layout_binding{};
	sampler_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	sampler_layout_binding.binding = 1; // this must be synced with the one in the shaders
	sampler_layout_binding.descriptorCount = 1;
	sampler_layout_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT; // defines which shader stage the descriptor is going to be referenced
	sampler_layout_binding.pImmutableSamplers = nullptr; // only relevant for image sampling related descriptors

	std::vector<VkDescriptorSetLayoutBinding> bindings{ ubo_layout_binding, sampler_layout_binding };
	VkDescriptorSetLayoutCreateInfo layout_info{};
	layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layout_info.bindingCount = static_cast<uint32_t>(bindings.size());
	layout_info.pBindings = bindings.data();

	if (vkCreateDescriptorSetLayout(logical_device, &layout_info, nullptr, &descriptor_set_layout) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create descriptor set layout!");
	}
}

void GraphicsEngine::create_descriptor_pools()
{
	VkDescriptorPoolSize uniform_buffer_pool_size{};
	uniform_buffer_pool_size.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	uniform_buffer_pool_size.descriptorCount = static_cast<uint32_t>(swap_chain_images.size());
	VkDescriptorPoolSize combined_image_sampler_pool_size{};
	combined_image_sampler_pool_size.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	combined_image_sampler_pool_size.descriptorCount = static_cast<uint32_t>(swap_chain_images.size());
	std::vector<VkDescriptorPoolSize> pool_sizes{ uniform_buffer_pool_size, combined_image_sampler_pool_size };

	// allocate a descriptor for ever image in the swap chain
	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = pool_sizes.size();
	poolInfo.pPoolSizes = pool_sizes.data();
	// defines maximum number of descriptor sets that may be allocated
	poolInfo.maxSets = static_cast<uint32_t>(swap_chain_images.size());

	if (vkCreateDescriptorPool(get_logical_device(), &poolInfo, nullptr, &descriptor_pool) != VK_SUCCESS)
	{
		throw std::runtime_error("GraphicsEngine::create_descriptor_pool: failed to create descriptor pool!");
	}
}

void GraphicsEngine::create_descriptor_sets()
{
	std::vector<VkDescriptorSetLayout> layouts(static_cast<uint32_t>(swap_chain_images.size()), descriptor_set_layout);
	VkDescriptorSetAllocateInfo alloc_info{};
	alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	alloc_info.descriptorPool = descriptor_pool;
	alloc_info.descriptorSetCount = static_cast<uint32_t>(swap_chain_images.size());
	alloc_info.pSetLayouts = layouts.data();

	descriptor_sets.resize(static_cast<uint32_t>(swap_chain_images.size()));

	// allocates all descriptor sets within the descriptor pool
	if (vkAllocateDescriptorSets(get_logical_device(), &alloc_info, descriptor_sets.data()) != VK_SUCCESS)
	{
		throw std::runtime_error("GraphicsEngine::create_descriptor_sets: failed to allocate descriptor sets!");
	}

	// configure the descriptors
	for (size_t i = 0; i < static_cast<uint32_t>(swap_chain_images.size()); i++)
	{
		VkDescriptorBufferInfo buffer_info{};
		buffer_info.buffer = uniform_buffers[i];
		buffer_info.offset = 0;
		buffer_info.range = sizeof(UniformBufferObject);

		VkDescriptorImageInfo image_info{};
		image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		image_info.imageView = texture_mgr.get_texture_image();
		image_info.sampler = texture_mgr.get_texture_sampler();

		std::vector<VkWriteDescriptorSet> descriptor_writes(2);
		descriptor_writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptor_writes[0].dstSet = descriptor_sets[i];
		descriptor_writes[0].dstBinding = 0; // also set to 0 in the shader
		descriptor_writes[0].dstArrayElement = 0; // offset
		descriptor_writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptor_writes[0].descriptorCount = 1;
		descriptor_writes[0].pBufferInfo = &buffer_info;
		descriptor_writes[0].pImageInfo = nullptr;
		descriptor_writes[0].pTexelBufferView = nullptr;

		descriptor_writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptor_writes[1].dstSet = descriptor_sets[i];
		descriptor_writes[1].dstBinding = 1; // also set to 1 in the shader
		descriptor_writes[1].dstArrayElement = 0; // offset
		descriptor_writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descriptor_writes[1].descriptorCount = 1;
		descriptor_writes[1].pBufferInfo = nullptr; 
		descriptor_writes[1].pImageInfo = &image_info;
		descriptor_writes[1].pTexelBufferView = nullptr;

		vkUpdateDescriptorSets(get_logical_device(), 
							   static_cast<uint32_t>(descriptor_writes.size()), 
							   descriptor_writes.data(), 
							   0, 
							   nullptr);
	}
}