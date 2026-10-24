#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>


bool write_rgba_png(const std::filesystem::path &path, uint32_t width, uint32_t height,
                    const std::vector<uint8_t> &rgba);
