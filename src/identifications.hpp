#pragma once

#include <cstdint>
#include <cstdlib>
#include <functional>
#include <cassert>


template<typename Tag>
struct GenericID
{
public:
	explicit GenericID() : id(0) {}
	explicit GenericID(uint64_t id) : id(id) {}

    auto operator<=>(const GenericID& rhs) const = default;
	GenericID operator++() { return GenericID(++id); }
	GenericID operator++(int) { return GenericID(id++); }

	uint64_t get_underlying() const { return id; }

	static uint64_t generate_new_id()
	{
		return global_id++;
	}

private:
	uint64_t id;
	static inline uint64_t global_id = 0;
};

template<typename Tag>
struct std::hash<GenericID<Tag>>
{
	std::size_t operator()(const GenericID<Tag>& id) const
	{
		return std::hash<uint64_t>()(id.get_underlying());
	}
};

using ObjectID = GenericID<class ObjectIDTag>;
using ShapeID = GenericID<class ShapeIDTag>;
using MeshID = GenericID<class MeshIDTag>;
using MaterialID = GenericID<class MaterialIDTag>;

// This ID is unique for each object in each frame
struct EntityFrameID
{
	EntityFrameID(ObjectID object_id, uint32_t frame_idx) :
		object_id(object_id),
		frame_idx(frame_idx)
	{
		assert(frame_idx < MAX_FRAMES);
	}

	auto operator<=>(const EntityFrameID&) const = default;

	uint64_t get_underlying() const
	{
		return MAX_FRAMES * object_id.get_underlying() + frame_idx;
	}

private:
	ObjectID object_id;
	uint8_t frame_idx;

	static constexpr int MAX_FRAMES = 3;
};

template<>
struct std::hash<EntityFrameID>
{
	std::size_t operator()(const EntityFrameID& id) const
	{
		return std::hash<uint64_t>()(id.get_underlying());
	}
};