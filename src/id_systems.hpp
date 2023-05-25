#pragma once

#include "objects/object_id.hpp"
#include "shapes/shape_id.hpp"

#include <cassert>
#include <compare>
#include <cstdint>
#include <stdexcept>
#include <functional>


// This ID is unique for each object in each frame
struct EntityFrameID
{
	EntityFrameID(ObjectID object_id, uint32_t frame_idx) :
		object_id(object_id),
		frame_idx(frame_idx)
	{
		assert(frame_idx < MAX_FRAME_IDX);
		if (object_id.get_underlying() >= MAX_OBJECTS)
		{
			throw std::runtime_error("EntityFrameID: ObjectID is out of range, increase 'MAX_OBJECTS'");
		}
	}

	constexpr auto operator<=>(const EntityFrameID&) const = default;

	uint64_t get_underlying() const
	{
		return frame_idx * MAX_OBJECTS + object_id.get_underlying();
	}

private:
	ObjectID object_id;
	uint8_t frame_idx;

	static constexpr int MAX_FRAME_IDX = 3;
	static constexpr int MAX_OBJECTS = 1000;
};

template<>
struct std::hash<EntityFrameID>
{
	std::size_t operator()(const EntityFrameID& id) const
	{
		return std::hash<uint64_t>()(id.get_underlying());
	}
};