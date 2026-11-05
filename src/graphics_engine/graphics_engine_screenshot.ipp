#pragma once

#include "graphics_engine_frame.hpp"

#include "graphics_engine.hpp"
#include "graphics_engine_swap_chain.hpp"
#include "dynamic_rendering.hpp"
#include "utility.hpp"
#include "frame_capture_pixels.hpp"
#include "frame_capture_png.hpp"

#include <vector>
#include <cstdint>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace
{
std::filesystem::path make_screenshot_path()
{
	const auto now = std::chrono::system_clock::now();
	const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
	const auto time_t = std::chrono::system_clock::to_time_t(now);

	std::tm tm{};
	localtime_r(&time_t, &tm);

	std::ostringstream oss;
	oss << std::put_time(&tm, "%Y%m%d_%H%M%S")
		<< "_"
		<< std::setfill('0') << std::setw(3) << millis.count()
		<< ".png";

	return Utility::get_writable_data_path() / "screenshots" / oss.str();
}

std::optional<CaptureChannelOrder> capture_channel_order(const VkFormat format)
{
	switch (format)
	{
	case VK_FORMAT_R8G8B8A8_UNORM:
	case VK_FORMAT_R8G8B8A8_SRGB:
		return CaptureChannelOrder::RGBA;
	case VK_FORMAT_B8G8R8A8_UNORM:
	case VK_FORMAT_B8G8R8A8_SRGB:
		return CaptureChannelOrder::BGRA;
	default:
		return std::nullopt;
	}
}

void record_presentation_readback(
	const VkCommandBuffer command_buffer,
	const VkImage image,
	const VkBuffer destination,
	const VkExtent2D extent)
{
	command_image_barrier(command_buffer, image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
		VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_READ_BIT);

	VkBufferImageCopy region{};
	region.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
	region.imageExtent = { extent.width, extent.height, 1 };
	vkCmdCopyImageToBuffer(command_buffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		destination, 1, &region);

	command_image_barrier(command_buffer, image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
		VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_READ_BIT,
		VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE);
}
}

void GraphicsEngineFrame::maybe_prepare_screenshot_capture()
{
	if (!get_graphics_engine().get_gui_manager().debug.consume_screenshot_request())
	{
		return;
	}

	screenshot_path = make_screenshot_path();
	std::filesystem::create_directories(screenshot_path.parent_path());
	screenshot_extent = swap_chain.get_extent();

	const VkDeviceSize buffer_size = static_cast<VkDeviceSize>(screenshot_extent.width) * screenshot_extent.height * 4;
	screenshot_staging_buffer.emplace(create_buffer(
		buffer_size,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		1,
		"screenshot_staging"));

	record_presentation_readback(command_buffer, presentation_image,
		screenshot_staging_buffer->get_buffer(), screenshot_extent);
}

void GraphicsEngineFrame::flush_screenshot_capture()
{
	if (!screenshot_staging_buffer.has_value())
	{
		return;
	}

	vkWaitForFences(get_logical_device(), 1, &fence_frame_inflight, VK_TRUE, std::numeric_limits<uint64_t>::max());

	const size_t pixel_bytes = static_cast<size_t>(screenshot_extent.width) * screenshot_extent.height * 4;
	void* mapped = screenshot_staging_buffer->map();
	std::vector<uint8_t> rgba;
	try
	{
		screenshot_staging_buffer->invalidate();
		const auto channel_order = capture_channel_order(swap_chain.get_image_format());
		if (!channel_order)
			throw std::runtime_error("screenshot readback encountered an unsupported swapchain format");
		rgba = convert_capture_pixels_to_rgba8(
			std::span(static_cast<const uint8_t*>(mapped), pixel_bytes), *channel_order);
	}
	catch (...)
	{
		screenshot_staging_buffer->unmap();
		throw;
	}
	screenshot_staging_buffer->unmap();

	write_rgba_png(screenshot_path, screenshot_extent.width, screenshot_extent.height, rgba);
	screenshot_staging_buffer.reset();
}

void GraphicsEngineFrame::maybe_prepare_frame_capture()
{
	auto* session = get_graphics_engine().get_frame_capture_session();
	if (!session)
		return;
	const auto target = session->get_target();
	if (!target || target->frame_number != get_graphics_engine().get_render_frame().frame_number)
		return;

	const auto format = swap_chain.get_image_format();
	if (!capture_channel_order(format))
	{
		session->fail(*target, "unsupported presentation format for RGBA8 capture");
		return;
	}
	frame_capture_target = target;
	frame_capture_extent = swap_chain.get_extent();
	const VkDeviceSize buffer_size = static_cast<VkDeviceSize>(frame_capture_extent.width)
		* frame_capture_extent.height * 4;
	frame_capture_staging_buffer.emplace(create_buffer(
		buffer_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		1, "frame_capture_staging"));

	record_presentation_readback(command_buffer, presentation_image,
		frame_capture_staging_buffer->get_buffer(), frame_capture_extent);
}

void GraphicsEngineFrame::flush_frame_capture()
{
	if (!frame_capture_staging_buffer || !frame_capture_target)
		return;
	auto* session = get_graphics_engine().get_frame_capture_session();
	const auto target = *frame_capture_target;
	bool fence_completed = false;
	try
	{
		const auto remaining = target.deadline - FrameCaptureSession::Clock::now();
		if (remaining <= FrameCaptureSession::Clock::duration::zero())
			throw std::runtime_error("presentation capture deadline expired before fence completion");
		const auto timeout_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(remaining).count();
		if (vkWaitForFences(get_logical_device(), 1, &fence_frame_inflight,
			VK_TRUE, static_cast<uint64_t>(timeout_ns)) != VK_SUCCESS)
			throw std::runtime_error("failed to wait for presentation capture fence");
		fence_completed = true;
		if (session->get_target() != target)
		{
			frame_capture_staging_buffer.reset();
			frame_capture_target.reset();
			return;
		}
		const size_t pixel_bytes = static_cast<size_t>(frame_capture_extent.width)
			* frame_capture_extent.height * 4;
		void* mapped = frame_capture_staging_buffer->map();
		std::vector<uint8_t> pixels;
		try
		{
			frame_capture_staging_buffer->invalidate();
			const auto channel_order = capture_channel_order(swap_chain.get_image_format());
			if (!channel_order)
				throw std::runtime_error("unsupported presentation format for RGBA8 capture");
			pixels = convert_capture_pixels_to_rgba8(
				std::span(static_cast<const uint8_t*>(mapped), pixel_bytes), *channel_order);
		}
		catch (...)
		{
			frame_capture_staging_buffer->unmap();
			throw;
		}
		frame_capture_staging_buffer->unmap();
		session->complete(target, FrameCaptureResult{
			.request_id = target.request_id,
			.frame_number = target.frame_number,
			.width = frame_capture_extent.width,
			.height = frame_capture_extent.height,
			.format = FrameCapturePixelFormat::RGBA8,
			.source_format = static_cast<uint32_t>(swap_chain.get_image_format()),
			.source_color_space = static_cast<uint32_t>(swap_chain.get_color_space()),
			.pixels = std::move(pixels),
			.status = FrameCaptureStatus::Success,
		});
	}
	catch (const std::exception& error)
	{
		session->fail(target, error.what());
		if (fence_completed)
		{
			frame_capture_staging_buffer.reset();
		}
		frame_capture_target.reset();
		throw;
	}
	frame_capture_staging_buffer.reset();
	frame_capture_target.reset();
}
