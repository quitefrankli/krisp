#pragma once

#include "constants.hpp"

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

	static GenericID generate_new_id()
	{
		return GenericID(global_id++);
	}

	static uint64_t get_next_id() { return global_id; }
	static void set_next_id(const uint64_t next_id) { global_id = next_id; }

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
using EntityID = ObjectID;
using MeshID = GenericID<class MeshIDTag>;
using MaterialID = GenericID<class MaterialIDTag>;
using RenderableID = GenericID<class RenderableIDTag>;
using SkeletonID = GenericID<class SkeletonIDTag>;
using AnimationID = GenericID<class AnimationIDTag>;

// Radices for every bounded field after the leading ID. Cumulative positional
// strides are derived automatically. For fields [A, B, C], radices
// [B_count, C_count] produce:
// A * (B_count * C_count) + B * C_count + C.
template<uint64_t... Values>
struct ComplexIDRadices
{
	static constexpr int count = sizeof...(Values);
	static constexpr std::array<uint64_t, count> values = { Values... };
	static constexpr std::array<uint64_t, count> strides = []
	{
		std::array<uint64_t, count> result{};
		uint64_t cumulative = 1;
		for (size_t index = count; index-- > 0;)
		{
			cumulative *= values[index];
			result[index] = cumulative;
		}
		return result;
	}();
};

template<typename Radices, typename... IDTypes>
struct ComplexID
{
	// The final field has an implicit stride of one, so N fields require N - 1
	// radices. Callers must ensure each bounded field fits within its radix.
	using IDTypesTuple = std::tuple<IDTypes...>;
	static_assert(std::tuple_size_v<IDTypesTuple> == Radices::count + 1);

	ComplexID(IDTypes... ids) : ids(ids...) {}
	IDTypesTuple ids;

	auto operator<=>(const ComplexID&) const = default;

	template<int idx = 0, std::enable_if_t<!std::is_arithmetic_v<std::tuple_element_t<idx, IDTypesTuple>>, int> = 0>
	uint64_t get_underlying() const
	{
		if constexpr (idx == Radices::count)
		{
			return std::get<idx>(ids).get_underlying();
		} else
		{
			return std::get<idx>(ids).get_underlying() * Radices::strides[idx] + get_underlying<idx+1>();
		}
	}

	template<int idx = 0, std::enable_if_t<std::is_arithmetic_v<std::tuple_element_t<idx, IDTypesTuple>>, int> = 0>
	uint64_t get_underlying() const
	{
		if constexpr (idx == Radices::count)
		{
			return std::get<idx>(ids);
		} else
		{
			return std::get<idx>(ids) * Radices::strides[idx] + get_underlying<idx+1>();
		}
	}
};

template<typename... T>
struct std::hash<ComplexID<T...>>
{
	std::size_t operator()(const ComplexID<T...>& id) const
	{
		return std::hash<uint64_t>()(id.get_underlying());
	}
};

// Per-frame allocation identities. RenderableID and SkeletonID are immutable,
// never-reintroduced resource identities.
using RenderableFrameID = ComplexID<ComplexIDRadices<CSTS::UPPERBOUND_SWAPCHAIN_IMAGES>,
	RenderableID,
	uint32_t>;

using SkeletonFrameID = ComplexID<ComplexIDRadices<CSTS::UPPERBOUND_SWAPCHAIN_IMAGES>,
	SkeletonID,
	uint32_t>;
