#include "frame_capture_png.hpp"

#include <algorithm>
#include <fstream>


namespace
{
void append_u32_be(std::vector<uint8_t> &out, const uint32_t value)
{
	out.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
	out.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
	out.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
	out.push_back(static_cast<uint8_t>(value & 0xFF));
}

uint32_t crc32(const uint8_t *data, const size_t len)
{
	uint32_t crc = 0xFFFFFFFFu;
	for (size_t i = 0; i < len; ++i)
	{
		crc ^= data[i];
		for (int bit = 0; bit < 8; ++bit)
			crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
	}
	return ~crc;
}

uint32_t adler32(const uint8_t *data, const size_t len)
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

void append_png_chunk(std::vector<uint8_t> &png, const char type[4], const std::vector<uint8_t> &chunk_data)
{
	append_u32_be(png, static_cast<uint32_t>(chunk_data.size()));
	const size_t type_begin = png.size();
	for (size_t i = 0; i < 4; ++i)
		png.push_back(static_cast<uint8_t>(type[i]));
	png.insert(png.end(), chunk_data.begin(), chunk_data.end());
	append_u32_be(png, crc32(png.data() + type_begin, png.size() - type_begin));
}
} // namespace

bool write_rgba_png(const std::filesystem::path &path, const uint32_t width, const uint32_t height,
                    const std::vector<uint8_t> &rgba)
{
	if (rgba.size() != static_cast<size_t>(width) * height * 4)
		return false;

	std::vector<uint8_t> scanlines;
	scanlines.reserve(static_cast<size_t>(height) * (static_cast<size_t>(width) * 4 + 1));
	for (uint32_t y = 0; y < height; ++y)
	{
		scanlines.push_back(0);
		const size_t row_start = static_cast<size_t>(y) * width * 4;
		scanlines.insert(scanlines.end(), rgba.begin() + row_start,
		                 rgba.begin() + row_start + static_cast<size_t>(width) * 4);
	}

	std::vector<uint8_t> zlib;
	zlib.reserve(scanlines.size() + (scanlines.size() / 65535 + 1) * 5 + 6);
	zlib.push_back(0x78);
	zlib.push_back(0x01);
	size_t offset = 0;
	while (offset < scanlines.size())
	{
		const size_t remaining = scanlines.size() - offset;
		const uint16_t block_len = static_cast<uint16_t>(std::min<size_t>(remaining, 65535));
		zlib.push_back(offset + block_len == scanlines.size() ? 1 : 0);
		zlib.push_back(static_cast<uint8_t>(block_len & 0xFF));
		zlib.push_back(static_cast<uint8_t>((block_len >> 8) & 0xFF));
		const uint16_t nlen = static_cast<uint16_t>(~block_len);
		zlib.push_back(static_cast<uint8_t>(nlen & 0xFF));
		zlib.push_back(static_cast<uint8_t>((nlen >> 8) & 0xFF));
		zlib.insert(zlib.end(), scanlines.begin() + offset, scanlines.begin() + offset + block_len);
		offset += block_len;
	}
	append_u32_be(zlib, adler32(scanlines.data(), scanlines.size()));

	std::vector<uint8_t> png{137, 80, 78, 71, 13, 10, 26, 10};
	std::vector<uint8_t> ihdr;
	append_u32_be(ihdr, width);
	append_u32_be(ihdr, height);
	ihdr.insert(ihdr.end(), {8, 6, 0, 0, 0});
	append_png_chunk(png, "IHDR", ihdr);
	append_png_chunk(png, "IDAT", zlib);
	append_png_chunk(png, "IEND", {});

	std::ofstream file(path, std::ios::binary);
	if (!file.good())
		return false;
	file.write(reinterpret_cast<const char *>(png.data()), static_cast<std::streamsize>(png.size()));
	return file.good();
}
