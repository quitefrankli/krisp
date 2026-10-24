#include "frame_capture_pixels.hpp"

#include <stdexcept>


std::vector<uint8_t> convert_capture_pixels_to_rgba8(const std::span<const uint8_t> source,
                                                     const CaptureChannelOrder channel_order)
{
	if (source.size() % 4 != 0)
		throw std::invalid_argument("capture pixel data must contain complete 4-byte pixels");

	std::vector<uint8_t> rgba(source.size());
	for (size_t offset = 0; offset < source.size(); offset += 4)
	{
		if (channel_order == CaptureChannelOrder::RGBA)
		{
			rgba[offset + 0] = source[offset + 0];
			rgba[offset + 1] = source[offset + 1];
			rgba[offset + 2] = source[offset + 2];
		}
		else
		{
			rgba[offset + 0] = source[offset + 2];
			rgba[offset + 1] = source[offset + 1];
			rgba[offset + 2] = source[offset + 0];
		}
		rgba[offset + 3] = source[offset + 3];
	}
	return rgba;
}
