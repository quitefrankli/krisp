#pragma once

#include "graphics_engine.hpp"
#include "video_recorder.hpp"

#include "shared_data_structures.hpp"
#include "analytics.hpp"
#include "entity_component_system/mesh_system.hpp"
#include "entity_component_system/material_system.hpp"
#include "constants.hpp"
#include "renderable/material_group.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/string_cast.hpp>
#include <fmt/core.h>
#include <fmt/color.h>

#include <stdexcept>
#include <thread>
#include <chrono>
#include <vector>
#include <iostream>

GraphicsEngine::GraphicsEngine(App::Window& window_) :
	window(window_),
	instance(*this),
	validation_layer(*this),
	device(*this),
	texture_mgr(*this),
	rsrc_mgr(*this),
	renderer_mgr(*this),
	swap_chain(*this),
	pipeline_mgr(*this),
	// raytracing_component(*this),
	gui_manager(*this),
	video_recorder(std::make_unique<VideoRecorder>())
{
	FPS_tracker = std::make_unique<Analytics>("FPS Tracker",
		[this](float fps) {
			set_fps(fps = float(1e6) / fps);
		}, 1, CSTS::TRACKER_LOG_PERIOD_SECONDS);
}

GraphicsEngine::~GraphicsEngine()
{
	fmt::print("GraphicsEngine: cleaning up\n");
	vkDeviceWaitIdle(get_logical_device());
	renderables.clear();
	for (const auto& [id, frame_allocation_count] : graphics_skeleton_frame_counts)
	{
		for (uint32_t frame_index = 0;
			frame_index < frame_allocation_count; ++frame_index)
		{
			get_rsrc_mgr().free_buffer(SkeletonFrameID{id, frame_index});
		}
	}
	graphics_skeleton_frame_counts.clear();
	for (auto& resources : retirement_queue.release_all())
		release_retired_resources(std::move(resources));
}

SubmissionSerial GraphicsEngine::register_graphics_submission()
{
	if (last_submitted_serial == std::numeric_limits<SubmissionSerial>::max())
		throw std::overflow_error("GraphicsEngine: graphics submission serial overflow");
	return ++last_submitted_serial;
}

void GraphicsEngine::complete_graphics_submission(const SubmissionSerial serial)
{
	completed_submission_serial = std::max(completed_submission_serial, serial);
	for (auto& resources : retirement_queue.release_completed(completed_submission_serial))
		release_retired_resources(std::move(resources));
}

QueueFamilyIndices GraphicsEngine::findQueueFamilies(VkPhysicalDevice device) {
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

	if (queueFamilyCount == 0)
	{
		throw std::runtime_error("failed to find any vulkan queue families!");
	}

	// if there is only one queue family, we can use it for both graphics and present
	if (queueFamilyCount == 1)
	{
		if (!(queueFamilies[0].queueFlags & VK_QUEUE_GRAPHICS_BIT) || !check_present_support(0))
		{
			throw std::runtime_error("single queue family does not support graphics or present operations!");
		}

		indices.graphicsFamily = 0;
		indices.presentFamily = 0;
		return indices;
	}

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

void GraphicsEngine::run() {
	try {
		Analytics analytics(
			"GraphicsEngine: avg loop processing period (excluding sleep)",
			CSTS::TRACKER_LOG_PERIOD_SECONDS);
		FPS_tracker->start();
		Utility::LoopSleeper loop_sleeper(std::chrono::milliseconds(17));
		while (!should_shutdown.load(std::memory_order_acquire))
		{
			// for FPS
			FPS_tracker->stop();
			FPS_tracker->start();

			analytics.start();

			accept_latest_render_frame();
			retire_unused_resources();
			if (!accepted_render_frame)
			{
#ifndef DISABLE_SLEEP
				loop_sleeper();
#endif
				continue;
			}

			gui_manager.draw();

			// raytracing_component.process();
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

void GraphicsEngine::accept_latest_render_frame()
{
	const auto publication = load_latest_completed_render_frames();
	if (!publication || publication->current == accepted_render_frame)
		return;

	const RenderFramePtr next_frame = publication->current;
	for (const auto& state : next_frame->renderables)
		if (!state.definition)
			throw std::runtime_error("GraphicsEngine: renderable definition is empty");
	for (const auto& pose : next_frame->skeletons)
		if (!pose.definition)
			throw std::runtime_error("GraphicsEngine: skeleton definition is empty");
	bool topology_changed = !accepted_render_frame
		|| accepted_render_frame->renderables.size() != next_frame->renderables.size()
		|| accepted_render_frame->skeletons.size() != next_frame->skeletons.size();
	if (!topology_changed)
	{
		for (const auto& state : next_frame->renderables)
			if (!renderables.contains(state.definition->id))
			{
				topology_changed = true;
				break;
			}
	}
	if (!topology_changed)
	{
		for (const auto& pose : next_frame->skeletons)
			if (!graphics_skeleton_frame_counts.contains(pose.definition->id))
			{
				topology_changed = true;
				break;
			}
	}

	accepted_render_frame = next_frame;
	renderable_indices.clear();
	renderable_indices.reserve(accepted_render_frame->renderables.size());
	for (uint32_t index = 0; index < accepted_render_frame->renderables.size(); ++index)
	{
		const auto& state = accepted_render_frame->renderables[index];
		if (!renderable_indices.emplace(state.definition->id, index).second)
			throw std::runtime_error("GraphicsEngine: duplicate renderable ID");
	}

	render_skeleton_indices.clear();
	render_skeleton_indices.reserve(accepted_render_frame->skeletons.size());
	for (uint32_t index = 0; index < accepted_render_frame->skeletons.size(); ++index)
	{
		const auto& pose = accepted_render_frame->skeletons[index];
		if (!render_skeleton_indices.emplace(pose.definition->id, index).second)
			throw std::runtime_error("GraphicsEngine: duplicate skeleton ID");
	}
	for (const auto& state : accepted_render_frame->renderables)
		if (state.definition->skeleton_id
			&& !render_skeleton_indices.contains(*state.definition->skeleton_id))
		{
			throw std::runtime_error("GraphicsEngine: renderable references a missing skeleton");
		}

	if (!topology_changed)
		return;

	reconcile_topology(*accepted_render_frame);

}

void GraphicsEngine::reconcile_topology(const RenderFrame& frame)
{
	RetiredGraphicsResources retired;

	std::unordered_set<RenderableID> renderable_ids;
	renderable_ids.reserve(frame.renderables.size());
	for (const auto& state : frame.renderables)
		renderable_ids.insert(state.definition->id);
	std::unordered_set<SkeletonID> skeleton_ids;
	skeleton_ids.reserve(frame.skeletons.size());
	for (const auto& pose : frame.skeletons)
		skeleton_ids.insert(pose.definition->id);

	retired.renderables.reserve(renderables.size());
	for (auto it = renderables.begin(); it != renderables.end();)
	{
		if (renderable_ids.contains(it->first))
		{
			++it;
			continue;
		}
		auto removed = renderables.extract(it++);
		retired.renderables.push_back(removed.mapped()->take_graphics_resources());
	}

	retired.skeletons.reserve(graphics_skeleton_frame_counts.size());
	for (auto it = graphics_skeleton_frame_counts.begin();
		it != graphics_skeleton_frame_counts.end();)
	{
		if (skeleton_ids.contains(it->first))
		{
			++it;
			continue;
		}
		auto removed = graphics_skeleton_frame_counts.extract(it++);
		retired.skeletons.push_back(
			GraphicsSkeletonResources{removed.key(), removed.mapped()});
	}
	enqueue_retirement(std::move(retired));

	for (const auto& pose : frame.skeletons)
	{
		const SkeletonID id = pose.definition->id;
		if (graphics_skeleton_frame_counts.contains(id))
			continue;
		const uint32_t frame_allocation_count = get_num_swapchain_images();
		const size_t size = sizeof(SDS::Bone) * pose.definition->bones.size();
		uint32_t reserved_frame_count = 0;
		try
		{
			for (; reserved_frame_count < frame_allocation_count; ++reserved_frame_count)
			{
				get_rsrc_mgr().reserve_buffer(
					SkeletonFrameID{id, reserved_frame_count}, size);
			}
		}
		catch (...)
		{
			for (uint32_t frame_index = 0;
				frame_index < reserved_frame_count; ++frame_index)
			{
				get_rsrc_mgr().free_buffer(
					SkeletonFrameID{id, frame_index});
			}
			throw;
		}
		graphics_skeleton_frame_counts.emplace(id, frame_allocation_count);
	}

	for (const auto& state : frame.renderables)
	{
		if (renderables.contains(state.definition->id))
			continue;

		auto graphics_renderable =
			std::make_unique<GraphicsRenderable>(*this, state.definition);
		create_renderable_buffers(*graphics_renderable);
		create_renderable_dsets(*graphics_renderable);
		renderables.emplace(state.definition->id, std::move(graphics_renderable));
	}

	draw_lists.rebuild(renderables);
}

void GraphicsEngine::retire_unused_resources()
{
	auto retired_materials = get_material_system().take_retired();
	auto retired_meshes = get_mesh_system().take_retired();
	if (retired_materials.empty() && retired_meshes.empty())
		return;

	RetiredGraphicsResources retired;
	retired.materials = std::move(retired_materials);
	retired.meshes = std::move(retired_meshes);
	enqueue_retirement(std::move(retired));
}

void GraphicsEngine::enqueue_retirement(RetiredGraphicsResources resources)
{
	if (resources.empty())
		return;
	retirement_queue.enqueue(last_submitted_serial, std::move(resources));
	for (auto& completed : retirement_queue.release_completed(completed_submission_serial))
		release_retired_resources(std::move(completed));
}

void GraphicsEngine::release_retired_resources(RetiredGraphicsResources resources)
{
	for (auto& renderable : resources.renderables)
	{
		for (uint32_t frame_index = 0;
			frame_index < renderable.frame_allocation_count; ++frame_index)
		{
			get_rsrc_mgr().free_buffer(
				RenderableFrameID{renderable.id, frame_index});
		}
		if (renderable.dset != VK_NULL_HANDLE)
			get_rsrc_mgr().free_dset(renderable.dset);
		get_rsrc_mgr().free_dsets(renderable.frame_dsets);
	}
	for (const auto& skeleton : resources.skeletons)
	{
		for (uint32_t frame_index = 0;
			frame_index < skeleton.frame_allocation_count; ++frame_index)
		{
			get_rsrc_mgr().free_buffer(
				SkeletonFrameID{skeleton.id, frame_index});
		}
	}
	for (const MaterialID id : resources.materials)
	{
		get_rsrc_mgr().free_buffer(id);
		get_texture_mgr().free_texture(id);
	}
	for (const MeshID id : resources.meshes)
		get_rsrc_mgr().free_buffer(id);
}

int GraphicsEngine::find_memory_type(uint32_t type_filter, VkMemoryPropertyFlags flags)
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

void GraphicsEngine::recreate_swap_chain()
{
	// TODO:
	// Reimplement this, or don't since resizing window is very low priority

	// // for when window is minimised
	// int width = 0, height = 0;
    // while (width == 0 || height == 0) {
    //     glfwGetFramebufferSize(get_window(), &width, &height);
    //     glfwWaitEvents();
    // }

	// swap_chain.reset();
}

VkCommandBuffer GraphicsEngine::begin_single_time_commands()
{
	VkCommandBuffer commandBuffer = get_rsrc_mgr().create_command_buffer();
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

 	// start recording the command buffer
    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    return commandBuffer;
}

void GraphicsEngine::end_single_time_commands(VkCommandBuffer command_buffer)
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

VkExtent2D GraphicsEngine::get_extent()
{
	return GraphicsEngineSwapChain::get_extent(get_physical_device(), get_window_surface());
}

void GraphicsEngine::create_image(uint32_t width,
											   uint32_t height,
											   VkFormat format,
											   VkImageTiling tiling,
											   VkImageUsageFlags usage,
											   VkMemoryPropertyFlags properties,
											   VkImage &image,
											   VkDeviceMemory &image_memory,
											   VkSampleCountFlagBits sample_count_flag,
											   const uint32_t layer_count,
											   const VkImageCreateFlags flags,
											   const uint32_t mip_levels)
{
	VkImageCreateInfo image_info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
	image_info.imageType = VK_IMAGE_TYPE_2D; // 1D for array of data or gradient, 3D for voxels
	image_info.extent.width = static_cast<uint32_t>(width);
	image_info.extent.height = static_cast<uint32_t>(height);
	image_info.extent.depth = 1;
	image_info.mipLevels = mip_levels;
	image_info.arrayLayers = layer_count; // for cube mapping
	image_info.format = format;
	image_info.tiling = tiling;							  // types include:
														  // LINEAR - texels are laid out in row major order
														  // OPTIMAL - texels are laid out in an implementation defined order
	image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; // UNDEFINED = not usable by GPU and first transition will discard texels
														  // PREINITIALIZED = not usable by GPU and first transition will preserve texels
	image_info.usage = usage;
	image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE; // will only be used by one queue family
	image_info.samples = sample_count_flag;			// for multisampling
	image_info.flags = flags;

	if (image_info.arrayLayers > 1)
	{
		// for the moment the only reason we would want arrayLayers>1 is when we want a cubemap
		assert(image_info.flags & VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT);
	}

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

VkImageView GraphicsEngine::create_image_view(
	VkImage& image,
	VkFormat format,
	VkImageAspectFlags aspect_flags,
	VkImageViewType view_type,
	const uint32_t layer_count,
	const uint32_t mip_levels)
{
	VkImageViewCreateInfo create_info{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
	create_info.image = image;
	create_info.viewType = view_type; // specifies how the image data should be interpreted
												  // i.e. treat images as 1D, 2D, 3D textures and cube maps
	create_info.format = format;
	create_info.subresourceRange.aspectMask = aspect_flags; // describes image purpose and which part should be accessed
	create_info.subresourceRange.baseMipLevel = 0;
	create_info.subresourceRange.levelCount = mip_levels;
	create_info.subresourceRange.baseArrayLayer = 0;
	create_info.subresourceRange.layerCount = layer_count;

	VkImageView image_view;
	if (vkCreateImageView(get_logical_device(), &create_info, nullptr, &image_view) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create image views!");
	}

	return image_view;
}

void GraphicsEngine::transition_image_layout(
	VkImage image,
	VkImageLayout old_layout,
	VkImageLayout new_layout,
	VkCommandBuffer command_buffer,
	const uint32_t layer_count,
	const uint32_t mip_levels)
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
	barrier.subresourceRange.levelCount = mip_levels;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = layer_count;
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

App::Window& GraphicsEngine::get_window()
{
	return window;
}

void GraphicsEngine::copy_buffer(VkBuffer src_buffer, VkBuffer dest_buffer, size_t size)
{
	// memory transfer operations are executed using command buffers

	VkCommandBuffer command_buffer = begin_single_time_commands();

	// actual copy command
	VkBufferCopy copy_region{};
	copy_region.size = size;
	vkCmdCopyBuffer(command_buffer, src_buffer, dest_buffer, 1, &copy_region);

	end_single_time_commands(command_buffer);
}

VkFormat GraphicsEngine::find_depth_format()
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
			// As opposed to something like VK_FORMAT_D32_SFLOAT
			// we want the S8_UINT bit for stencil buffer
			{VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
			VK_IMAGE_TILING_OPTIMAL,
			VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
	}

	return depth_format.value();
}


void GraphicsEngine::create_renderable_buffers(GraphicsRenderable &graphics_renderable)
{
	// vertex buffer doesn't change per frame so unlike uniform buffer it doesn't need to be
	// per frame resource and therefore we only need 1 copy
	auto &rsrc_mgr = get_rsrc_mgr();
	const auto &renderable = graphics_renderable.get_definition();
	// reserve and write to mesh buffer (actually vertex and index buffers)
	const auto &mesh = renderable.get_mesh();
	rsrc_mgr.write_to_buffer(mesh.get_id(), mesh);

	// reserve and write to materials buffer
	switch (renderable.pipeline_render_type)
	{
	case ERenderType::COLOR:
	case ERenderType::SKINNED_COLOR: {
		const FlatMatGroup flat_mat_group(renderable.material_owners);
		const auto *material = dynamic_cast<const ColorMaterial *>(&renderable.get_material(0));
		if (!material)
			throw std::runtime_error(fmt::format("GraphicsEngine: material {} is not a ColorMaterial",
			                                     flat_mat_group.color_mat.get_underlying()));
		rsrc_mgr.write_to_buffer(flat_mat_group.color_mat, material->data);
		break;
	}
	default:
		break;
	}

	// these buffers are dynamic (changing between frames) and therefore requires duplicate buffers per swapchain image
	const uint32_t frame_allocation_count = get_num_swapchain_images();
	for (uint32_t frame_idx = 0; frame_idx < frame_allocation_count; ++frame_idx)
	{
		rsrc_mgr.reserve_buffer(
			RenderableFrameID{graphics_renderable.get_id(), frame_idx},
			sizeof(SDS::ObjectData));
		graphics_renderable.record_frame_allocation();
	}

	// Ray-tracing buffer mapping is unsupported:
	// SDS::BufferMapEntry buffer_map;
	// buffer_map.vertex_offset =
	//     rsrc_mgr.get_vertex_buffer_offset(renderable.get_mesh_id());
	// buffer_map.index_offset =
	//     rsrc_mgr.get_index_buffer_offset(renderable.get_mesh_id());
}

void GraphicsEngine::create_renderable_dsets(GraphicsRenderable &graphics_renderable)
{
	const std::vector<VkDescriptorSetLayout> frame_layouts(
		graphics_renderable.get_frame_allocation_count(),
		get_rsrc_mgr().get_per_renderable_frame_dset_layout());
	graphics_renderable.set_frame_dsets(get_rsrc_mgr().reserve_dsets(frame_layouts));
	const auto skeleton_id = graphics_renderable.get_skeleton_id();
	for (uint32_t frame_idx = 0;
		frame_idx < graphics_renderable.get_frame_allocation_count(); ++frame_idx)
	{
		const VkDescriptorSet new_descriptor_set =
			graphics_renderable.get_frame_dset(frame_idx);
		std::vector<VkWriteDescriptorSet> descriptor_writes;

		VkDescriptorBufferInfo buffer_info{};
		const GraphicsBuffer::Slot buffer_slot =
			get_rsrc_mgr().get_buffer_slot(
				RenderableFrameID{graphics_renderable.get_id(), frame_idx});
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

		if (skeleton_id)
		{
			const GraphicsBuffer::Slot bone_slot =
				get_rsrc_mgr().get_buffer_slot(SkeletonFrameID{*skeleton_id, frame_idx});
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

		vkUpdateDescriptorSets(get_logical_device(), descriptor_writes.size(), descriptor_writes.data(), 0, nullptr);
	}

	// TODO: we need to cache the dsets for each material/texture
	const RenderableDefinition &renderable = graphics_renderable.get_definition();
	// TODO: we need to split the renderable dset layout to a material only one and a texture only one
	// however this is a lot of work and will involve creating a new pipeline
	VkDescriptorSet new_descriptor_set = get_rsrc_mgr().reserve_dset(get_rsrc_mgr().get_renderable_dset_layout());
	graphics_renderable.set_dset(new_descriptor_set);
	std::vector<VkWriteDescriptorSet> descriptor_writes;

	// TODO: after resolving above todo, need to move this within the below switch statement
	const GraphicsBuffer::Slot mat_slot = [&]() {
		if (renderable.pipeline_render_type != ERenderType::COLOR &&
		    renderable.pipeline_render_type != ERenderType::SKINNED_COLOR)
		{
			// TODO: this needs to be properly fixed
			GraphicsBuffer::Slot slot;
			slot.offset = 0;
			slot.size = 4; // this is just a dummy value
			return slot;
		}

		const FlatMatGroup flat_material_group(renderable.material_owners);
		return get_rsrc_mgr().get_buffer_slot(flat_material_group.color_mat);
	}();
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

	switch (renderable.pipeline_render_type)
	{
	case ERenderType::CUBEMAP: {
		VkDescriptorImageInfo image_info{};
		image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		// some useful links when we get up to this part
		// https://gamedev.stackexchange.com/questions/146982/compressed-vs-uncompressed-textures-differences
		// https://stackoverflow.com/questions/27345340/how-do-i-render-multiple-textures-in-modern-opengl
		// for texture seams and more indepth texture atlas
		// https://www.pluralsight.com/blog/film-games/understanding-uvs-love-them-or-hate-them-theyre-essential-to-know
		// descriptor set layout frequency
		// https://stackoverflow.com/questions/50986091/what-is-the-best-way-of-dealing-with-textures-for-a-same-shader-in-vulkan
		const CubeMapMatGroup cube_map_mat_group(renderable.material_owners);
		const GraphicsEngineTexture &texture = get_texture_mgr().fetch_cubemap_texture(cube_map_mat_group);
		image_info.imageView = texture.get_texture_image_view();
		image_info.sampler = texture.get_texture_sampler();

		VkWriteDescriptorSet combined_image_sampler_descriptor_set{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
		combined_image_sampler_descriptor_set.dstSet = new_descriptor_set;
		combined_image_sampler_descriptor_set.dstBinding = SDS::RASTERIZATION_ALBEDO_TEXTURE_DATA_BINDING;
		combined_image_sampler_descriptor_set.dstArrayElement = 0; // offset
		combined_image_sampler_descriptor_set.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		combined_image_sampler_descriptor_set.descriptorCount = 1;
		combined_image_sampler_descriptor_set.pImageInfo = &image_info;
		descriptor_writes.push_back(combined_image_sampler_descriptor_set);

		vkUpdateDescriptorSets(get_logical_device(), static_cast<uint32_t>(descriptor_writes.size()),
		                       descriptor_writes.data(), 0, nullptr);
		break;
	}
	case ERenderType::STANDARD:
	case ERenderType::SKINNED: {
		VkDescriptorImageInfo image_info{};
		image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		const TexturedMatGroup textured_mat_group(renderable.material_owners);
		const GraphicsEngineTexture &texture =
			get_texture_mgr().fetch_texture(textured_mat_group.get_material_owner(textured_mat_group.base_color_mat),
		                                    ETextureSamplerType::ADDR_MODE_REPEAT);
		image_info.imageView = texture.get_texture_image_view();
		image_info.sampler = texture.get_texture_sampler();

		VkWriteDescriptorSet combined_image_sampler_descriptor_set{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
		combined_image_sampler_descriptor_set.dstSet = new_descriptor_set;
		combined_image_sampler_descriptor_set.dstBinding = SDS::RASTERIZATION_ALBEDO_TEXTURE_DATA_BINDING;
		combined_image_sampler_descriptor_set.dstArrayElement = 0; // offset
		combined_image_sampler_descriptor_set.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		combined_image_sampler_descriptor_set.descriptorCount = 1;
		combined_image_sampler_descriptor_set.pImageInfo = &image_info;
		descriptor_writes.push_back(combined_image_sampler_descriptor_set);

		VkDescriptorImageInfo normal_image_info{};
		normal_image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		const GraphicsEngineTexture &normal_texture =
			textured_mat_group.normal_mat.has_value()
				? get_texture_mgr().fetch_texture(textured_mat_group.get_material_owner(*textured_mat_group.normal_mat),
		                                          ETextureSamplerType::ADDR_MODE_REPEAT)
				: get_texture_mgr().fetch_flat_normal_texture();
		normal_image_info.imageView = normal_texture.get_texture_image_view();
		normal_image_info.sampler = normal_texture.get_texture_sampler();

		VkWriteDescriptorSet normal_sampler_descriptor{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
		normal_sampler_descriptor.dstSet = new_descriptor_set;
		normal_sampler_descriptor.dstBinding = SDS::RASTERIZATION_NORMAL_TEXTURE_DATA_BINDING;
		normal_sampler_descriptor.dstArrayElement = 0;
		normal_sampler_descriptor.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		normal_sampler_descriptor.descriptorCount = 1;
		normal_sampler_descriptor.pImageInfo = &normal_image_info;
		descriptor_writes.push_back(normal_sampler_descriptor);

		VkDescriptorImageInfo specular_info{};
		specular_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		const GraphicsEngineTexture &specular_texture =
			textured_mat_group.specular_mat ? get_texture_mgr().fetch_texture(textured_mat_group.get_material_owner(
																				  *textured_mat_group.specular_mat),
		                                                                      ETextureSamplerType::ADDR_MODE_REPEAT)
											: get_texture_mgr().fetch_white_texture();
		specular_info.imageView = specular_texture.get_texture_image_view();
		specular_info.sampler = specular_texture.get_texture_sampler();
		VkWriteDescriptorSet specular_descriptor{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
		specular_descriptor.dstSet = new_descriptor_set;
		specular_descriptor.dstBinding = SDS::RASTERIZATION_SPECULAR_TEXTURE_DATA_BINDING;
		specular_descriptor.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		specular_descriptor.descriptorCount = 1;
		specular_descriptor.pImageInfo = &specular_info;
		descriptor_writes.push_back(specular_descriptor);

		vkUpdateDescriptorSets(get_logical_device(), static_cast<uint32_t>(descriptor_writes.size()),
		                       descriptor_writes.data(), 0, nullptr);
		break;
	}
	default:
		vkUpdateDescriptorSets(get_logical_device(), static_cast<uint32_t>(descriptor_writes.size()),
		                       descriptor_writes.data(), 0, nullptr);
	}

}
