#pragma once

#include <cstdint>


// maps object id to starting offset in the vertex, index and uniform buffers
struct BufferMapEntry
{
	uint32_t vertex_offset;
	uint32_t index_offset;
	uint32_t uniform_offset;
};