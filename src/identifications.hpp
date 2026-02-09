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
using SkeletonID = GenericID<class SkeletonIDTag>;
using AnimationID = GenericID<class AnimationIDTag>;

template<uint64_t... Capacities>
struct ComplexIDCapacities
{
	static constexpr int cap_size = sizeof...(Capacities);
	static constexpr std::array<uint64_t, cap_size> capacities = { Capacities... };
};

template<typename Capacities, typename... IDTypes>
struct ComplexID
{
	// An ID that is composed of multiple IDs
	using IDTypesTuple = std::tuple<IDTypes...>;
    static_assert(std::tuple_size_v<IDTypesTuple> == Capacities::cap_size + 1);

    ComplexID(IDTypes... ids) : ids(ids...) {}
    IDTypesTuple ids;

	auto operator<=>(const ComplexID&) const = default;

    template<int idx = 0, std::enable_if_t<!std::is_arithmetic_v<std::tuple_element_t<idx, IDTypesTuple>>, int> = 0>
    uint64_t get_underlying() const
	{
		if constexpr (idx == Capacities::cap_size)
		{
			return std::get<idx>(ids).get_underlying();
		} else
		{
			return std::get<idx>(ids).get_underlying() * Capacities::capacities[idx] + get_underlying<idx+1>();
		}
	}

	template<int idx = 0, std::enable_if_t<std::is_arithmetic_v<std::tuple_element_t<idx, IDTypesTuple>>, int> = 0>
	uint64_t get_underlying() const
    {
        if constexpr (idx == Capacities::cap_size)
        {
            return std::get<idx>(ids);
        } else
        {
            return std::get<idx>(ids) * Capacities::capacities[idx] + get_underlying<idx+1>();
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

using EntityFrameID = ComplexID<ComplexIDCapacities<CSTS::UPPERBOUND_SWAPCHAIN_IMAGES>, 
								ObjectID, 
								uint32_t>;
using SkeletonFrameID = ComplexID<ComplexIDCapacities<CSTS::UPPERBOUND_SWAPCHAIN_IMAGES>,
								  SkeletonID, 
								  uint32_t>;