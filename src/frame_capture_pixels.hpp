#pragma once

#include <cstdint>
#include <span>
#include <vector>


enum class CaptureChannelOrder
{
	RGBA,
	BGRA,
};

std::vector<uint8_t> convert_capture_pixels_to_rgba8(std::span<const uint8_t> source,
                                                     CaptureChannelOrder channel_order);
