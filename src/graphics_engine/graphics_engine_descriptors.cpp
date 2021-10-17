#include "graphics_engine.hpp"

#include "uniform_buffer_object.hpp"
#include "objects.hpp"

/*
	a descriptor layout is used to "drag" out descriptor sets from a descriptor pool
	

*/

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

	if (vkCreateDescriptorSetLayout(get_logical_device(), &layout_info, nullptr, &descriptor_set_layout) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create descriptor set layout!");
	}
}

void GraphicsEngine::create_descriptor_pools()
{
	// 1x uniform descriptor per descriptor set
	VkDescriptorPoolSize uniform_buffer_pool_size{};
	uniform_buffer_pool_size.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	uniform_buffer_pool_size.descriptorCount = get_num_swapchain_images() * MAX_NUM_DESCRIPTOR_SETS;

	// 1x texture descriptor per descriptor set
	VkDescriptorPoolSize combined_image_sampler_pool_size{};
	combined_image_sampler_pool_size.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	combined_image_sampler_pool_size.descriptorCount = get_num_swapchain_images() * MAX_NUM_DESCRIPTOR_SETS;
	std::vector<VkDescriptorPoolSize> pool_sizes{ uniform_buffer_pool_size, combined_image_sampler_pool_size };

	// allocate a descriptor for every image in the swap chain
	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = pool_sizes.size();
	poolInfo.pPoolSizes = pool_sizes.data();
	// defines maximum number of descriptor sets that may be allocated
	poolInfo.maxSets = get_num_swapchain_images() * MAX_NUM_DESCRIPTOR_SETS;

	if (vkCreateDescriptorPool(get_logical_device(), &poolInfo, nullptr, &descriptor_pool) != VK_SUCCESS)
	{
		throw std::runtime_error("GraphicsEngine::create_descriptor_pool: failed to create descriptor pool!");
	}
}

// void GraphicsEngine::create_descriptor_sets()
// {
// 	std::vector<VkDescriptorSetLayout> layouts(static_cast<uint32_t>(swap_chain_images.size()) * get_vertex_sets().size(), descriptor_set_layout);
// 	VkDescriptorSetAllocateInfo alloc_info{};
// 	alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
// 	alloc_info.descriptorPool = descriptor_pool;
// 	alloc_info.descriptorSetCount = static_cast<uint32_t>(swap_chain_images.size()) * get_vertex_sets().size();
// 	alloc_info.pSetLayouts = layouts.data();

// 	descriptor_sets.resize(static_cast<uint32_t>(swap_chain_images.size()) * get_vertex_sets().size());

// 	// allocates all descriptor sets within the descriptor pool
// 	if (vkAllocateDescriptorSets(get_logical_device(), &alloc_info, descriptor_sets.data()) != VK_SUCCESS)
// 	{
// 		throw std::runtime_error("GraphicsEngine::create_descriptor_sets: failed to allocate descriptor sets!");
// 	}

// 	// configure the descriptors
// 	int global_descriptor_sets_offset = 0; // this is really bad TODO: make this better
// 	for (int swap_chain_index = 0; swap_chain_index < static_cast<uint32_t>(swap_chain_images.size()); swap_chain_index++)
// 	{
// 		for (int object_index = 0; object_index < get_objects().size(); object_index++)
// 		{
//  			// create a descriptor set for every vertex set, TODO: this is quite inefficient as a single object contains multiple vertex sets
// 			Object* object = get_objects()[object_index];
// 			std::vector<std::vector<Vertex>>& cur_vertex_sets = object->vertex_sets;

// 			for (int vertex_set_index = 0; vertex_set_index < cur_vertex_sets.size(); vertex_set_index++)
// 			{
// 				VkDescriptorBufferInfo buffer_info{};
// 				buffer_info.buffer = uniform_buffers[swap_chain_index];
// 				buffer_info.offset = sizeof(UniformBufferObject) * object_index;
// 				buffer_info.range = sizeof(UniformBufferObject);

// 				VkWriteDescriptorSet uniform_buffer_descriptor_set{};
// 				uniform_buffer_descriptor_set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
// 				uniform_buffer_descriptor_set.dstSet = descriptor_sets[global_descriptor_sets_offset];
// 				uniform_buffer_descriptor_set.dstBinding = 0; // also set to 0 in the shader
// 				uniform_buffer_descriptor_set.dstArrayElement = 0; // offset
// 				uniform_buffer_descriptor_set.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
// 				uniform_buffer_descriptor_set.descriptorCount = 1;
// 				uniform_buffer_descriptor_set.pBufferInfo = &buffer_info;
// 				uniform_buffer_descriptor_set.pImageInfo = nullptr;
// 				uniform_buffer_descriptor_set.pTexelBufferView = nullptr;

// 				VkDescriptorImageInfo image_info{};
// 				image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
// 				// some useful links when we get up to this part
// 				// https://gamedev.stackexchange.com/questions/146982/compressed-vs-uncompressed-textures-differences
// 				// https://stackoverflow.com/questions/27345340/how-do-i-render-multiple-textures-in-modern-opengl
// 				// for texture seams and more indepth texture atlas https://www.pluralsight.com/blog/film-games/understanding-uvs-love-them-or-hate-them-theyre-essential-to-know
// 				// descriptor set layout frequency https://stackoverflow.com/questions/50986091/what-is-the-best-way-of-dealing-with-textures-for-a-same-shader-in-vulkan
// 				image_info.imageView = texture_mgr.get_texture_image(); // TODO: move this to object
// 				image_info.sampler = texture_mgr.get_texture_sampler(); // TODO: move this to object

// 				VkWriteDescriptorSet combined_image_sampler_descriptor_set{};
// 				combined_image_sampler_descriptor_set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
// 				combined_image_sampler_descriptor_set.dstSet = descriptor_sets[global_descriptor_sets_offset];
// 				combined_image_sampler_descriptor_set.dstBinding = 1; // also set to 1 in the shader
// 				combined_image_sampler_descriptor_set.dstArrayElement = 0; // offset
// 				combined_image_sampler_descriptor_set.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
// 				combined_image_sampler_descriptor_set.descriptorCount = 1;
// 				combined_image_sampler_descriptor_set.pBufferInfo = nullptr; 
// 				combined_image_sampler_descriptor_set.pImageInfo = &image_info;
// 				combined_image_sampler_descriptor_set.pTexelBufferView = nullptr;

// 				std::vector<VkWriteDescriptorSet> descriptor_writes{ uniform_buffer_descriptor_set, combined_image_sampler_descriptor_set };

// 				vkUpdateDescriptorSets(get_logical_device(), 
// 									static_cast<uint32_t>(descriptor_writes.size()), 
// 									descriptor_writes.data(), 
// 									0, 
// 									nullptr);

// 				global_descriptor_sets_offset++;
// 			}
// 		}
// 	}
// }



//
// FROM DYNAMIC TEXTURE CHANGE
//

// void GraphicsEngine::create_descriptor_sets()
// {
// 	std::vector<VkDescriptorSetLayout> layouts(static_cast<uint32_t>(swap_chain_images.size()) * get_vertex_sets().size(), descriptor_set_layout);
// 	VkDescriptorSetAllocateInfo alloc_info{};
// 	alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
// 	alloc_info.descriptorPool = descriptor_pool;
// 	alloc_info.descriptorSetCount = static_cast<uint32_t>(swap_chain_images.size()) * get_vertex_sets().size();
// 	alloc_info.pSetLayouts = layouts.data();

// 	descriptor_sets.resize(static_cast<uint32_t>(swap_chain_images.size()) * get_vertex_sets().size());

// 	// allocates all descriptor sets within the descriptor pool
// 	if (vkAllocateDescriptorSets(get_logical_device(), &alloc_info, descriptor_sets.data()) != VK_SUCCESS)
// 	{
// 		throw std::runtime_error("GraphicsEngine::create_descriptor_sets: failed to allocate descriptor sets!");
// 	}

// 	update_descriptor_sets();
// }

// void GraphicsEngine::update_descriptor_sets()
// {
// 	// configure the descriptors
// 	int global_descriptor_sets_offset = 0; // this is really bad TODO: make this better
// 	for (int swap_chain_index = 0; swap_chain_index < static_cast<uint32_t>(swap_chain_images.size()); swap_chain_index++)
// 	{
// 		for (int object_index = 0; object_index < get_objects().size(); object_index++)
// 		{
//  			// create a descriptor set for every vertex set, TODO: this is quite inefficient as a single object contains multiple vertex sets
// 			Object* object = get_objects()[object_index];
// 			std::vector<std::vector<Vertex>>& cur_vertex_sets = object->vertex_sets;

// 			for (int vertex_set_index = 0; vertex_set_index < cur_vertex_sets.size(); vertex_set_index++)
// 			{
// 				VkDescriptorBufferInfo buffer_info{};
// 				buffer_info.buffer = uniform_buffers[swap_chain_index];
// 				buffer_info.offset = sizeof(UniformBufferObject) * object_index;
// 				buffer_info.range = sizeof(UniformBufferObject);

// 				VkWriteDescriptorSet uniform_buffer_descriptor_set{};
// 				uniform_buffer_descriptor_set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
// 				uniform_buffer_descriptor_set.dstSet = descriptor_sets[global_descriptor_sets_offset];
// 				uniform_buffer_descriptor_set.dstBinding = 0; // also set to 0 in the shader
// 				uniform_buffer_descriptor_set.dstArrayElement = 0; // offset
// 				uniform_buffer_descriptor_set.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
// 				uniform_buffer_descriptor_set.descriptorCount = 1;
// 				uniform_buffer_descriptor_set.pBufferInfo = &buffer_info;
// 				uniform_buffer_descriptor_set.pImageInfo = nullptr;
// 				uniform_buffer_descriptor_set.pTexelBufferView = nullptr;

// 				VkDescriptorImageInfo image_info{};
// 				image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
// 				// some useful links when we get up to this part
// 				// https://gamedev.stackexchange.com/questions/146982/compressed-vs-uncompressed-textures-differences
// 				// https://stackoverflow.com/questions/27345340/how-do-i-render-multiple-textures-in-modern-opengl
// 				// for texture seams and more indepth texture atlas https://www.pluralsight.com/blog/film-games/understanding-uvs-love-them-or-hate-them-theyre-essential-to-know
// 				// descriptor set layout frequency https://stackoverflow.com/questions/50986091/what-is-the-best-way-of-dealing-with-textures-for-a-same-shader-in-vulkan
// 				image_info.imageView = texture_mgr.get_texture_image(); // TODO: move this to object
// 				image_info.sampler = texture_mgr.get_texture_sampler(); // TODO: move this to object

// 				VkWriteDescriptorSet combined_image_sampler_descriptor_set{};
// 				combined_image_sampler_descriptor_set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
// 				combined_image_sampler_descriptor_set.dstSet = descriptor_sets[global_descriptor_sets_offset];
// 				combined_image_sampler_descriptor_set.dstBinding = 1; // also set to 1 in the shader
// 				combined_image_sampler_descriptor_set.dstArrayElement = 0; // offset
// 				combined_image_sampler_descriptor_set.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
// 				combined_image_sampler_descriptor_set.descriptorCount = 1;
// 				combined_image_sampler_descriptor_set.pBufferInfo = nullptr; 
// 				combined_image_sampler_descriptor_set.pImageInfo = &image_info;
// 				combined_image_sampler_descriptor_set.pTexelBufferView = nullptr;

// 				std::vector<VkWriteDescriptorSet> descriptor_writes{ uniform_buffer_descriptor_set, combined_image_sampler_descriptor_set };

// 				vkUpdateDescriptorSets(get_logical_device(), 
// 									static_cast<uint32_t>(descriptor_writes.size()), 
// 									descriptor_writes.data(), 
// 									0, 
// 									nullptr);

// 				global_descriptor_sets_offset++;
// 			}
// 		}
// 	}
// }
