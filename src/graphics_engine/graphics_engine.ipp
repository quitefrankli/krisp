#pragma once

#include "graphics_engine.hpp"

#include "camera.hpp"
#include "game_engine.hpp"
#include "objects/object.hpp"
#include "shared_data_structures.hpp"
#include "analytics.hpp"
#include "entity_component_system/ecs.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/string_cast.hpp>
#include <fmt/core.h>
#include <fmt/color.h>

#include <stdexcept>
#include <thread>
#include <chrono>
#include <vector>
#include <iostream>


template<typename GameEngineT>
GraphicsEngine<GameEngineT>::GraphicsEngine(GameEngineT& game_engine) : 
	game_engine(game_engine),
	instance(*this),
	validation_layer(*this),
	device(*this),
	texture_mgr(*this),
	rsrc_mgr(*this),
	renderer_mgr(*this),
	swap_chain(*this),
	pipeline_mgr(*this),
	raytracing_component(*this),
	gui_manager(*this)
{
	FPS_tracker = std::make_unique<Analytics>(
		[this](float fps) {
			set_fps(fps = 1e6 / fps);
		}, 1);
	FPS_tracker->text = "FPS Tracker";
}

template<typename GameEngineT>
GraphicsEngine<GameEngineT>::~GraphicsEngine() 
{
	fmt::print("GraphicsEngine: cleaning up\n");
	vkDeviceWaitIdle(get_logical_device());
	objects.clear(); // must be cleared before the logical device is destroyed
}

template<typename GameEngineT>
Camera* GraphicsEngine<GameEngineT>::get_camera()
{
	return &(game_engine.get_camera());
}

template<typename GameEngineT>
QueueFamilyIndices GraphicsEngine<GameEngineT>::findQueueFamilies(VkPhysicalDevice device) {
	QueueFamilyIndices indices;
	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

	std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

	const auto check_present_support = [&](const uint32_t qFamilyIndex)
	{
		VkBool32 present_support = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(device, qFamilyIndex, get_window_surface(), &present_support);
		return present_support;
	};

	for (uint32_t i = 0; i < queueFamilies.size(); i++)
	{
		if (!indices.graphicsFamily.has_value() && queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) // GRAPHICS_BIT also implicitly supports VK_QUEUE_TRANSFER_BIT
		{
			indices.graphicsFamily = i;
			continue;
		}
		if (!indices.presentFamily.has_value() && check_present_support(i))
		{
			indices.presentFamily = i;
			continue;
		}
	}

	return indices;
}

template<typename GameEngineT>
void GraphicsEngine<GameEngineT>::run() {
	try {
		Analytics analytics(60);
		analytics.text = "GraphicsEngine: avg loop processing period (excluding sleep)";
		FPS_tracker->start();
		Utility::LoopSleeper loop_sleeper(std::chrono::milliseconds(17));
		while (!should_shutdown)
		{
			// for FPS
			FPS_tracker->stop();
			FPS_tracker->start();

			analytics.start();

			ge_cmd_q_mutex.lock();
			while (!ge_cmd_q.empty())
			{
				ge_cmd_q.front()->process(this);
				ge_cmd_q.pop();
			}
			ge_cmd_q_mutex.unlock();

			gui_manager.draw();

			raytracing_component.process();

			swap_chain.draw();

			analytics.stop();

#ifndef DISABLE_SLEEP
			loop_sleeper();
#endif

		}
    } catch (const std::exception& e) {
		fmt::print(fg(fmt::color::red), "GraphicsEngine Exception Thrown!: {}\n", e.what());
        throw e;
	} catch (...) {
		fmt::print(fg(fmt::color::red), "GraphicsEngine Exception Thrown!: UNKNOWN\n");
        throw std::runtime_error("");
	}
}

template<typename GameEngineT>
int GraphicsEngine<GameEngineT>::find_memory_type(uint32_t type_filter, VkMemoryPropertyFlags flags)
{
	VkPhysicalDeviceMemoryProperties memory_properties;
	vkGetPhysicalDeviceMemoryProperties(get_physical_device(), &memory_properties);

	for (uint32_t i = 0; i < memory_properties.memoryTypeCount; i++)
	{
		if ((type_filter & (1 << i)) && ((memory_properties.memoryTypes[i].propertyFlags & flags) == flags))
		{
			return i;
		}
	}

	throw std::runtime_error("failed to find suitable memory type!");
};

template<typename GameEngineT>
ECS& GraphicsEngine<GameEngineT>::get_ecs()
{
	return game_engine.get_ecs();
}

template<typename GameEngineT>
const ECS& GraphicsEngine<GameEngineT>::get_ecs() const
{
	return game_engine.get_ecs();
}

template<typename GameEngineT>
void GraphicsEngine<GameEngineT>::cleanup_entity(const ObjectID id)
{
	get_rsrc_mgr().free_vertex_buffer(id);
	get_rsrc_mgr().free_index_buffer(id);
	auto& obj = get_object(id);
	for (const auto& shape : obj.get_shapes())
	{
		get_rsrc_mgr().free_materials_buffer(shape.get_id());
	}
	for (uint32_t frame_idx = 0; frame_idx < get_num_swapchain_images(); ++frame_idx)
	{
		EntityFrameID efid{id, frame_idx};
		get_rsrc_mgr().free_uniform_buffer(efid);
		if (obj.get_render_type() == EPipelineType::SKINNED)
		{
			get_rsrc_mgr().free_bone_buffer(efid);
		}
	}
	objects.erase(id);
	++num_objs_deleted;
}

template<typename GameEngineT>
void GraphicsEngine<GameEngineT>::recreate_swap_chain()
{
	// TODO:
	// Reimplement this, or don't since resizing window is very low priority

	// // for when window is minimised
	// int width = 0, height = 0;
    // while (width == 0 || height == 0) {
    //     glfwGetFramebufferSize(get_window(), &width, &height);
    //     glfwWaitEvents();
    // }

	// vkDeviceWaitIdle(get_logical_device()); // we want to wait until resource is no longer in use

	// swap_chain.reset();
}

template<typename GameEngineT>
void GraphicsEngine<GameEngineT>::enqueue_cmd(std::unique_ptr<GraphicsEngineCommand>&& cmd)
{
	std::lock_guard<std::mutex> lock(ge_cmd_q_mutex);
	ge_cmd_q.push(std::move(cmd));
}

template<typename GameEngineT>
VkCommandBuffer GraphicsEngine<GameEngineT>::begin_single_time_commands()
{
	VkCommandBuffer commandBuffer = get_rsrc_mgr().create_command_buffer();
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

 	// start recording the command buffer
    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    return commandBuffer;
}

template<typename GameEngineT>
void GraphicsEngine<GameEngineT>::end_single_time_commands(VkCommandBuffer command_buffer)
{
    vkEndCommandBuffer(command_buffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &command_buffer;

	// note that unlike draw stage, we don't need to wait for anything here except for the queue to become idle
    vkQueueSubmit(graphics_queue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphics_queue);

    vkFreeCommandBuffers(get_logical_device(), get_command_pool(), 1, &command_buffer);
}

template<typename GameEngineT>
VkExtent2D GraphicsEngine<GameEngineT>::get_extent()
{
	return GraphicsEngineSwapChain<GraphicsEngine>::get_extent(get_physical_device(), get_window_surface());
}

template<typename GameEngineT>
void GraphicsEngine<GameEngineT>::update_command_buffer()
{
	swap_chain.update_command_buffer();
}

template<typename GameEngineT>
void GraphicsEngine<GameEngineT>::create_image(uint32_t width,
								uint32_t height,
								VkFormat format,
								VkImageTiling tiling,
								VkImageUsageFlags usage,
								VkMemoryPropertyFlags properties,
								VkImage &image,
								VkDeviceMemory &image_memory,
								VkSampleCountFlagBits sample_count_flag)
{
	VkImageCreateInfo image_info{};
	image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	image_info.imageType = VK_IMAGE_TYPE_2D; // 1D for array of data or gradient, 3D for voxels
	image_info.extent.width = static_cast<uint32_t>(width);
	image_info.extent.height = static_cast<uint32_t>(height);
	image_info.extent.depth = 1;
	image_info.mipLevels = 1; // mip mapping
	image_info.arrayLayers = 1;
	image_info.format = format;
	image_info.tiling = tiling;							  // types include:
														  // LINEAR - texels are laid out in row major order
														  // OPTIMAL - texels are laid out in an implementation defined order
	image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; // UNDEFINED = not usable by GPU and first transition will discard texels
														  // PREINITIALIZED = not usable by GPU and first transition will preserve texels
	image_info.usage = usage;
	image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE; // will only be used by one queue family
	image_info.samples = sample_count_flag;			// for multisampling
	image_info.flags = 0;

	if (vkCreateImage(get_logical_device(), &image_info, nullptr, &image) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create image!");
	}

	//
	// allocate memory for an image
	//

	VkMemoryRequirements mem_req;
	vkGetImageMemoryRequirements(get_logical_device(), image, &mem_req);
	VkMemoryAllocateInfo alloc_info{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
	alloc_info.allocationSize = mem_req.size;
	alloc_info.memoryTypeIndex = find_memory_type(mem_req.memoryTypeBits, properties);

	if (vkAllocateMemory(get_logical_device(), &alloc_info, nullptr, &image_memory) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to allocate image memory!");
	}

	vkBindImageMemory(get_logical_device(), image, image_memory, 0);
}

template<typename GameEngineT>
VkImageView GraphicsEngine<GameEngineT>::create_image_view(VkImage& image,
															VkFormat format,
															VkImageAspectFlags aspect_flags,
															VkImageViewType view_type)
{
	VkImageViewCreateInfo create_info{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
	create_info.image = image;
	create_info.viewType = view_type; // specifies how the image data should be interpreted
												  // i.e. treat images as 1D, 2D, 3D textures and cube maps
	create_info.format = format;
	create_info.subresourceRange.aspectMask = aspect_flags; // describes image purpose and which part should be accessed
	create_info.subresourceRange.baseMipLevel = 0;
	create_info.subresourceRange.levelCount = 1;
	create_info.subresourceRange.baseArrayLayer = 0;
	create_info.subresourceRange.layerCount = 1;

	VkImageView image_view;
	if (vkCreateImageView(get_logical_device(), &create_info, nullptr, &image_view) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create image views!");
	}

	return image_view;
}

template<typename GameEngineT>
void GraphicsEngine<GameEngineT>::transition_image_layout(
	VkImage image,
	VkImageLayout old_layout,
	VkImageLayout new_layout,
	VkCommandBuffer command_buffer)
{
	const bool is_external_command_buffer = command_buffer != nullptr;
	if (!command_buffer)
	{
		command_buffer = begin_single_time_commands();
	}

	// pipeline barrier to synchronize access to resources
	VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
	barrier.oldLayout = old_layout;
	barrier.newLayout = new_layout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image; // specifies image affeced
	// subresourceRange specifies what part of the image is affected
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseMipLevel = 0; // image is an array with no mip mapping
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;
	barrier.srcAccessMask = 0;
	barrier.dstAccessMask = 0;

	// transition types:
	//  * undefined -> transfer destination: transfer writes that don't need to wait on anything
	//  * transfer destination -> shader reading: shader reads should wait on transfer writes
	//		specifically the shader reads in the fragment shader
	const auto check_transition = [old_layout, new_layout](VkImageLayout x, VkImageLayout y) -> bool
	{
		return old_layout == x && new_layout == y;
	};
	VkPipelineStageFlags sourceStage, destinationStage;
	if (check_transition(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)) 
	{
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	} else if (check_transition(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)) 
	{
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	} else if (check_transition(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL))
	{
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = 0;
		sourceStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
		destinationStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
	} else if (check_transition(VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL))
	{
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		sourceStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
		destinationStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
	} else if (check_transition(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR))
	{
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = 0;
		sourceStage = VK_ACCESS_TRANSFER_WRITE_BIT;
		destinationStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
	} else if (check_transition(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL))
	{
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		barrier.dstAccessMask = 0;
		sourceStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
		destinationStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
	} else 
	{
		throw std::runtime_error("unsupported image layout transition!");
	}

	vkCmdPipelineBarrier(
		command_buffer,
		sourceStage, // which pipeline stage the operation should occur before the barrier
		destinationStage, // pipeline stage in which the operation will wait on the barrier
		0, // 
		0,
		nullptr,
		0,
		nullptr,
		1,
		&barrier
	);

	if (!is_external_command_buffer)
	{
		end_single_time_commands(command_buffer);
	}
}

template<typename GameEngineT>
App::Window& GraphicsEngine<GameEngineT>::get_window()
{
	return game_engine.get_window();
}

template<typename GameEngineT>
void GraphicsEngine<GameEngineT>::copy_buffer(VkBuffer src_buffer, VkBuffer dest_buffer, size_t size)
{
	// memory transfer operations are executed using command buffers

	VkCommandBuffer command_buffer = begin_single_time_commands();

	// actual copy command
	VkBufferCopy copy_region{};
	copy_region.size = size;
	vkCmdCopyBuffer(command_buffer, src_buffer, dest_buffer, 1, &copy_region);

	end_single_time_commands(command_buffer);
}

template<typename GameEngineT>
VkFormat GraphicsEngine<GameEngineT>::find_depth_format()
{
	if (!depth_format)
	{
		const auto find_supported_format = [&](
			std::vector<VkFormat> candidates, 
			VkImageTiling tiling, 
			VkFormatFeatureFlags features)
		{
			for (VkFormat format : candidates)
			{
				VkFormatProperties props;
				vkGetPhysicalDeviceFormatProperties(get_physical_device(), format, &props);
				if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features)
				{
					return format;
				}

				if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features)
				{
					return format;
				}
			}

			throw std::runtime_error("failed to find supported format!");
		};

		depth_format = find_supported_format(
			{VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
			VK_IMAGE_TILING_OPTIMAL,
			VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
	}

	return depth_format.value();
}


template<typename GameEngineT>
void GraphicsEngine<GameEngineT>::spawn_object_create_buffers(GraphicsEngineObject<GraphicsEngine>& graphics_object)
{
	// vertex buffer doesn't change per frame so unlike uniform buffer it doesn't need to be 
	// per frame resource and therefore we only need 1 copy
	const auto id = graphics_object.get_id();
	get_rsrc_mgr().reserve_vertex_buffer(id, graphics_object.get_game_object().get_vertices_data_size());
	get_rsrc_mgr().reserve_index_buffer(id, graphics_object.get_game_object().get_indices_data_size());
	get_rsrc_mgr().write_shapes_to_buffers(id, graphics_object.get_shapes());

	for (const auto& shape : graphics_object.get_shapes())
	{
		// upload materials
		const SDS::MaterialData& material = shape.get_material().get_data();
		get_rsrc_mgr().reserve_materials_buffer(shape.get_id(), sizeof(material));
		get_rsrc_mgr().write_to_materials_buffer(shape.get_id(), material);
	}

	// these buffers are dynamic (changing between frames) and therefore requires duplicate buffers per swapchain image
	const uint32_t nFrames = get_num_swapchain_images();
	for (uint32_t frame_idx = 0; frame_idx < nFrames; ++frame_idx)
	{
		// allocate space for object uniform buffer
		get_rsrc_mgr().reserve_uniform_buffer(EntityFrameID{id, frame_idx}, sizeof(SDS::ObjectData));

		// allocate space for bone matrices if needed
		if (graphics_object.get_render_type() == EPipelineType::SKINNED)
		{
			const size_t bone_data_size = sizeof(SDS::Bone) * get_ecs().get_bones(id).size();
			get_rsrc_mgr().reserve_bone_buffer(EntityFrameID{id, frame_idx}, bone_data_size);
		}
	}

	SDS::BufferMapEntry buffer_map;
	buffer_map.vertex_offset = get_rsrc_mgr().get_vertex_buffer_offset(id);
	buffer_map.index_offset = get_rsrc_mgr().get_index_buffer_offset(id);
	// this is currently hardcoded to always use the first frame's uniform buffer
	// since the buffer map is only used for raytracing it's ignored for now
	// TODO: fix this
	buffer_map.uniform_offset = get_rsrc_mgr().get_uniform_buffer_offset(EntityFrameID{id, 0});

	get_rsrc_mgr().write_to_mapping_buffer(id, buffer_map);
}

template<typename GameEngineT>
void GraphicsEngine<GameEngineT>::spawn_object_create_dsets(GraphicsEngineObject<GraphicsEngine>& object)
{
	// dset vectors are pre allocated to the size of the swap chain
	const uint32_t nFrames = get_num_swapchain_images();
	object.get_dsets().resize(nFrames);

	// per object descriptor set
	// currently the resources that are per obj just happen to be purely dynamic and so all of them need a separate
	// buffer + dset for each frame
	for (uint32_t frame_idx = 0; frame_idx < nFrames; ++frame_idx)
	{
		VkDescriptorSet new_descriptor_set = get_rsrc_mgr().reserve_dset(get_rsrc_mgr().get_per_obj_dset_layout());
		std::vector<VkWriteDescriptorSet> descriptor_writes;

		VkDescriptorBufferInfo buffer_info{};
		const GraphicsBuffer::Slot buffer_slot = get_rsrc_mgr().get_uniform_buffer_slot(EntityFrameID{object.get_id(), frame_idx});
		buffer_info.buffer = get_rsrc_mgr().get_uniform_buffer();
		buffer_info.offset = buffer_slot.offset;
		buffer_info.range = buffer_slot.size;
		VkWriteDescriptorSet uniform_buffer_dset_write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
		uniform_buffer_dset_write.dstSet = new_descriptor_set;
		uniform_buffer_dset_write.dstBinding = SDS::RASTERIZATION_OBJECT_DATA_BINDING;
		uniform_buffer_dset_write.dstArrayElement = 0; // offset
		uniform_buffer_dset_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		uniform_buffer_dset_write.descriptorCount = 1;
		uniform_buffer_dset_write.pBufferInfo = &buffer_info;
		descriptor_writes.push_back(uniform_buffer_dset_write);

		if (object.get_render_type() == EPipelineType::SKINNED)
		{
			const GraphicsBuffer::Slot bone_slot = 
				get_rsrc_mgr().get_bone_buffer_slot(EntityFrameID{object.get_id(), frame_idx});
			VkDescriptorBufferInfo bone_buffer_info{};
			bone_buffer_info.buffer = get_rsrc_mgr().get_bone_buffer();
			bone_buffer_info.offset = bone_slot.offset;
			bone_buffer_info.range = bone_slot.size;
			VkWriteDescriptorSet bone_buffer_dset_write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
			bone_buffer_dset_write.dstSet = new_descriptor_set;
			bone_buffer_dset_write.dstBinding = SDS::RASTERIZATION_BONE_DATA_BINDING;
			bone_buffer_dset_write.dstArrayElement = 0;
			bone_buffer_dset_write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			bone_buffer_dset_write.descriptorCount = 1;
			bone_buffer_dset_write.pBufferInfo = &bone_buffer_info;
			descriptor_writes.push_back(bone_buffer_dset_write);
		}

		vkUpdateDescriptorSets(get_logical_device(), 
							descriptor_writes.size(), 
							descriptor_writes.data(), 
							0, 
							nullptr);
		object.set_dset(new_descriptor_set, frame_idx);
	}

	// per shape descriptor set
	// currently the resources that are per shape just happen to be purely static and so we only need 1 dset and buffer
	for (auto& shape : object.get_shapes())
	{
		VkDescriptorSet new_descriptor_set = get_rsrc_mgr().reserve_dset(get_rsrc_mgr().get_per_shape_dset_layout());
		std::vector<VkWriteDescriptorSet> descriptor_writes;

		const GraphicsBuffer::Slot mat_slot = 
			get_rsrc_mgr().get_materials_buffer_slot(shape.get_id());
		VkDescriptorBufferInfo material_buffer_info{};
		material_buffer_info.buffer = get_rsrc_mgr().get_materials_buffer();
		material_buffer_info.offset = mat_slot.offset;
		material_buffer_info.range = mat_slot.size;
		VkWriteDescriptorSet material_buffer_dset{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
		material_buffer_dset.dstSet = new_descriptor_set;
		material_buffer_dset.dstBinding = SDS::RASTERIZATION_MATERIAL_DATA_BINDING;
		material_buffer_dset.dstArrayElement = 0;
		material_buffer_dset.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		material_buffer_dset.descriptorCount = 1;
		material_buffer_dset.pBufferInfo = &material_buffer_info;
		descriptor_writes.push_back(material_buffer_dset);

		switch (object.get_render_type())
		{
			case EPipelineType::STANDARD:
			case EPipelineType::CUBEMAP:
			case EPipelineType::SKINNED:
			{
				VkDescriptorImageInfo image_info{};
				image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				// some useful links when we get up to this part
				// https://gamedev.stackexchange.com/questions/146982/compressed-vs-uncompressed-textures-differences
				// https://stackoverflow.com/questions/27345340/how-do-i-render-multiple-textures-in-modern-opengl
				// for texture seams and more indepth texture atlas https://www.pluralsight.com/blog/film-games/understanding-uvs-love-them-or-hate-them-theyre-essential-to-know
				// descriptor set layout frequency https://stackoverflow.com/questions/50986091/what-is-the-best-way-of-dealing-with-textures-for-a-same-shader-in-vulkan
				image_info.imageView = shape.get_material().get_texture().get_texture_image_view();
				image_info.sampler = shape.get_material().get_texture().get_texture_sampler();

				VkWriteDescriptorSet combined_image_sampler_descriptor_set{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
				combined_image_sampler_descriptor_set.dstSet = new_descriptor_set;
				combined_image_sampler_descriptor_set.dstBinding = SDS::RASTERIZATION_ALBEDO_TEXTURE_DATA_BINDING;
				combined_image_sampler_descriptor_set.dstArrayElement = 0; // offset
				combined_image_sampler_descriptor_set.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
				combined_image_sampler_descriptor_set.descriptorCount = 1;
				combined_image_sampler_descriptor_set.pImageInfo = &image_info;
				descriptor_writes.push_back(combined_image_sampler_descriptor_set);
				break;
			}
			default:
				break;
		}

		vkUpdateDescriptorSets(get_logical_device(),
							static_cast<uint32_t>(descriptor_writes.size()), 
							descriptor_writes.data(), 
							0, 
							nullptr);
		shape.set_dset(new_descriptor_set);
	}
}
