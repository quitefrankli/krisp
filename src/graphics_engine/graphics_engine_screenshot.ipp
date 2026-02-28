#pragma once

#include "graphics_engine_frame.hpp"

#include "graphics_engine.hpp"
#include "graphics_engine_swap_chain.hpp"
#include "utility.hpp"

#include <algorithm>
#include <vector>
#include <fstream>
#include <cstdint>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace
{
void append_u32_be(std::vector<uint8_t>& out, const uint32_t value)
{
	out.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
	out.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
	out.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
	out.push_back(static_cast<uint8_t>(value & 0xFF));
}

uint32_t crc32(const uint8_t* data, const size_t len)
{
	uint32_t crc = 0xFFFFFFFFu;
	for (size_t i = 0; i < len; ++i)
	{
		crc ^= data[i];
		for (int bit = 0; bit < 8; ++bit)
		{
			crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
		}
	}
	return ~crc;
}

uint32_t adler32(const uint8_t* data, const size_t len)
{
	constexpr uint32_t mod = 65521u;
	uint32_t s1 = 1;
	uint32_t s2 = 0;
	for (size_t i = 0; i < len; ++i)
	{
		s1 = (s1 + data[i]) % mod;
		s2 = (s2 + s1) % mod;
	}
	return (s2 << 16) | s1;
}

void append_png_chunk(std::vector<uint8_t>& png, const char type[4], const std::vector<uint8_t>& chunk_data)
{
	append_u32_be(png, static_cast<uint32_t>(chunk_data.size()));
	const size_t type_begin = png.size();
	png.push_back(static_cast<uint8_t>(type[0]));
	png.push_back(static_cast<uint8_t>(type[1]));
	png.push_back(static_cast<uint8_t>(type[2]));
	png.push_back(static_cast<uint8_t>(type[3]));
	png.insert(png.end(), chunk_data.begin(), chunk_data.end());
	append_u32_be(png, crc32(png.data() + type_begin, png.size() - type_begin));
}

bool write_rgba_png(const std::filesystem::path& path, const uint32_t width, const uint32_t height, const std::vector<uint8_t>& rgba)
{
	if (rgba.size() != static_cast<size_t>(width) * height * 4)
	{
		return false;
	}

	std::vector<uint8_t> scanlines;
	scanlines.reserve(static_cast<size_t>(height) * (static_cast<size_t>(width) * 4 + 1));
	for (uint32_t y = 0; y < height; ++y)
	{
		scanlines.push_back(0); // no filter
		const size_t row_start = static_cast<size_t>(y) * width * 4;
		scanlines.insert(scanlines.end(), rgba.begin() + row_start, rgba.begin() + row_start + static_cast<size_t>(width) * 4);
	}

	std::vector<uint8_t> zlib;
	zlib.reserve(scanlines.size() + (scanlines.size() / 65535 + 1) * 5 + 6);
	zlib.push_back(0x78);
	zlib.push_back(0x01); // no compression

	size_t offset = 0;
	while (offset < scanlines.size())
	{
		const size_t remaining = scanlines.size() - offset;
		const uint16_t block_len = static_cast<uint16_t>(std::min<size_t>(remaining, 65535));
		const uint8_t is_final = (offset + block_len == scanlines.size()) ? 1 : 0;
		zlib.push_back(is_final); // BFINAL + BTYPE=00
		zlib.push_back(static_cast<uint8_t>(block_len & 0xFF));
		zlib.push_back(static_cast<uint8_t>((block_len >> 8) & 0xFF));
		const uint16_t nlen = static_cast<uint16_t>(~block_len);
		zlib.push_back(static_cast<uint8_t>(nlen & 0xFF));
		zlib.push_back(static_cast<uint8_t>((nlen >> 8) & 0xFF));
		zlib.insert(zlib.end(), scanlines.begin() + offset, scanlines.begin() + offset + block_len);
		offset += block_len;
	}

	append_u32_be(zlib, adler32(scanlines.data(), scanlines.size()));

	std::vector<uint8_t> png;
	png.reserve(zlib.size() + 128);
	png.insert(png.end(), { 137, 80, 78, 71, 13, 10, 26, 10 });

	std::vector<uint8_t> ihdr;
	ihdr.reserve(13);
	append_u32_be(ihdr, width);
	append_u32_be(ihdr, height);
	ihdr.push_back(8); // bit depth
	ihdr.push_back(6); // RGBA
	ihdr.push_back(0); // compression
	ihdr.push_back(0); // filter
	ihdr.push_back(0); // interlace
	append_png_chunk(png, "IHDR", ihdr);
	append_png_chunk(png, "IDAT", zlib);
	append_png_chunk(png, "IEND", {});

	std::ofstream file(path, std::ios::binary);
	if (!file.good())
	{
		return false;
	}
	file.write(reinterpret_cast<const char*>(png.data()), static_cast<std::streamsize>(png.size()));
	return file.good();
}

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

	return Utility::get_top_level_path() / "screenshots" / oss.str();
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
		0,
		0,
		nullptr,
		0,
		nullptr,
		1,
		&to_transfer);

	VkBufferImageCopy region{};
	region.bufferOffset = 0;
	region.bufferRowLength = 0;
	region.bufferImageHeight = 0;
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount = 1;
	region.imageOffset = {0, 0, 0};
	region.imageExtent = { screenshot_extent.width, screenshot_extent.height, 1 };
	vkCmdCopyImageToBuffer(
		command_buffer,
		presentation_image,
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		screenshot_staging_buffer->get_buffer(),
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
		0,
		0,
		nullptr,
		0,
		nullptr,
		1,
		&to_present);
}

void GraphicsEngineFrame::flush_screenshot_capture()
{
	if (!screenshot_staging_buffer.has_value())
	{
		return;
	}

	vkWaitForFences(get_logical_device(), 1, &fence_frame_inflight, VK_TRUE, std::numeric_limits<uint64_t>::max());

	const size_t pixel_bytes = static_cast<size_t>(screenshot_extent.width) * screenshot_extent.height * 4;
	const VkDeviceMemory memory = screenshot_staging_buffer->get_memory();
	void* mapped = nullptr;
	if (vkMapMemory(get_logical_device(), memory, 0, pixel_bytes, 0, &mapped) != VK_SUCCESS || !mapped)
	{
		throw std::runtime_error("GraphicsEngineFrame::flush_screenshot_capture: failed to map screenshot buffer");
	}

	std::vector<uint8_t> rgba(pixel_bytes);
	const auto* bgra = static_cast<const uint8_t*>(mapped);
	for (size_t i = 0; i < pixel_bytes; i += 4)
	{
		rgba[i + 0] = bgra[i + 2];
		rgba[i + 1] = bgra[i + 1];
		rgba[i + 2] = bgra[i + 0];
		rgba[i + 3] = bgra[i + 3];
	}
	vkUnmapMemory(get_logical_device(), memory);

	write_rgba_png(screenshot_path, screenshot_extent.width, screenshot_extent.height, rgba);
	screenshot_staging_buffer->destroy(get_logical_device());
	screenshot_staging_buffer.reset();
}
