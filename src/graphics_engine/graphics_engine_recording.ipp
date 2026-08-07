#pragma once

#include "graphics_engine_frame.hpp"
#include "graphics_engine.hpp"
#include "graphics_engine_swap_chain.hpp"
#include "video_recorder.hpp"
#include "utility.hpp"

#include <chrono>
#include <iomanip>
#include <sstream>
#include <limits>

namespace
{
std::filesystem::path make_recording_path()
{
	const auto now = std::chrono::system_clock::now();
	const auto time_t = std::chrono::system_clock::to_time_t(now);

	std::tm tm{};
	localtime_r(&time_t, &tm);

	std::ostringstream oss;
	oss << std::put_time(&tm, "%Y%m%d_%H%M%S") << ".mp4";

	return Utility::get_top_level_path() / "recordings" / oss.str();
}
}

void GraphicsEngineFrame::maybe_prepare_recording_capture()
{
	auto& debug = get_graphics_engine().get_gui_manager().debug;
	auto& recorder = get_graphics_engine().get_video_recorder();
	auto& session = get_graphics_engine().get_recording_session();

	const auto start_request = debug.consume_start_recording_request();
	if (start_request && !recorder.is_recording())
	{
		const VkExtent2D ext = swap_chain.get_extent();
		try
		{
			recorder.start(
				make_recording_path(), ext.width, ext.height,
				static_cast<int>(*start_request));
			session.start(*start_request);
			debug.set_is_recording(true);
		}
		catch (...)
		{
			session.stop();
			if (recorder.is_recording())
			{
				try { recorder.stop(); } catch (...) {}
			}
			debug.set_is_recording(false);
			throw;
		}
	}

	if (debug.consume_stop_recording_request())
	{
		session.stop();
		if (recorder.is_recording())
			recorder.stop();
		debug.set_is_recording(false);
		return;
	}

	if (!recorder.is_recording())
	{
		return;
	}
	const auto target = session.get_capture_target();
	if (!target)
		return;
	const uint64_t accepted_frame_number =
		get_graphics_engine().get_render_frame().frame_number;
	if (accepted_frame_number < target->render_frame_number)
		return;
	if (accepted_frame_number > target->render_frame_number)
	{
		session.stop();
		debug.set_is_recording(false);
		recorder.stop();
		throw std::logic_error("Recording capture target was skipped");
	}

	const VkExtent2D extent = swap_chain.get_extent();
	const VkDeviceSize buffer_size = static_cast<VkDeviceSize>(extent.width) * extent.height * 4;

	if (!recording_staging_buffer.has_value())
	{
		recording_staging_buffer.emplace(create_buffer(
			buffer_size,
			VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			1,
			"recording_staging"));
		recording_extent = extent;
	}
	else if (recording_extent.width != extent.width
		|| recording_extent.height != extent.height)
	{
		session.stop();
		debug.set_is_recording(false);
		recorder.stop();
		throw std::runtime_error("Recording resolution changed during capture");
	}

	VkImageMemoryBarrier to_transfer{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
	to_transfer.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	to_transfer.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	to_transfer.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	to_transfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	to_transfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	to_transfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	to_transfer.image = presentation_image;
	to_transfer.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	to_transfer.subresourceRange.baseMipLevel = 0;
	to_transfer.subresourceRange.levelCount = 1;
	to_transfer.subresourceRange.baseArrayLayer = 0;
	to_transfer.subresourceRange.layerCount = 1;

	vkCmdPipelineBarrier(
		command_buffer,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		0, 0, nullptr, 0, nullptr, 1, &to_transfer);

	VkBufferImageCopy region{};
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.layerCount = 1;
	region.imageExtent = { extent.width, extent.height, 1 };
	vkCmdCopyImageToBuffer(
		command_buffer,
		presentation_image,
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		recording_staging_buffer->get_buffer(),
		1,
		&region);

	VkImageMemoryBarrier to_present{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
	to_present.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	to_present.dstAccessMask = 0;
	to_present.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	to_present.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	to_present.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	to_present.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	to_present.image = presentation_image;
	to_present.subresourceRange = to_transfer.subresourceRange;

	vkCmdPipelineBarrier(
		command_buffer,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
		0, 0, nullptr, 0, nullptr, 1, &to_present);

	recording_has_pending_frame = true;
	recording_capture_target = target;
}

void GraphicsEngineFrame::flush_recording_capture()
{
	if (!recording_staging_buffer.has_value())
	{
		return;
	}

	auto& recorder = get_graphics_engine().get_video_recorder();
	auto& session = get_graphics_engine().get_recording_session();

	if (recording_has_pending_frame && recorder.is_recording())
	{
		try
		{
			if (vkWaitForFences(
					get_logical_device(), 1, &fence_frame_inflight, VK_TRUE,
					std::numeric_limits<uint64_t>::max()) != VK_SUCCESS)
				throw std::runtime_error("Failed to wait for recording capture");

			const size_t pixel_bytes = static_cast<size_t>(recording_extent.width)
				* recording_extent.height * 4;
			void* mapped = nullptr;
			if (vkMapMemory(
					get_logical_device(), recording_staging_buffer->get_memory(),
					0, pixel_bytes, 0, &mapped) != VK_SUCCESS || !mapped)
				throw std::runtime_error("Failed to map recording capture");
			try
			{
				recorder.submit_frame(
					static_cast<const uint8_t*>(mapped),
					recording_extent.width, recording_extent.height);
			}
			catch (...)
			{
				vkUnmapMemory(get_logical_device(), recording_staging_buffer->get_memory());
				throw;
			}
			vkUnmapMemory(get_logical_device(), recording_staging_buffer->get_memory());

			if (recording_capture_target)
				session.complete_capture(*recording_capture_target);
		}
		catch (...)
		{
			session.stop();
			get_graphics_engine().get_gui_manager().debug.set_is_recording(false);
			try { recorder.stop(); } catch (...) {}
			recording_has_pending_frame = false;
			recording_capture_target.reset();
			throw;
		}
	}

	recording_has_pending_frame = false;
	recording_capture_target.reset();

	if (!recorder.is_recording())
	{
		recording_staging_buffer->destroy(get_logical_device());
		recording_staging_buffer.reset();
	}
}
