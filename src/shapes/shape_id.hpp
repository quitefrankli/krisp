#pragma once

#include <cstdint>
#include <cstdlib>
#include <functional>


struct ShapeID
{
public:
	explicit ShapeID(uint64_t id) : id(id) {}

	bool operator==(const ShapeID& other) const { return id == other.id; }
	bool operator!=(const ShapeID& other) const { return id != other.id; }
	bool operator<(const ShapeID& other) const { return id < other.id; }
	bool operator>(const ShapeID& other) const { return id > other.id; }

	ShapeID operator++() { return ShapeID(++id); }
	ShapeID operator++(int) { return ShapeID(id++); }

	uint64_t get_underlying() const { return id; }

private:
	uint64_t id;
};

template<>
struct std::hash<ShapeID>
{
	std::size_t operator()(const ShapeID& id) const
	{
		return std::hash<uint64_t>()(id.get_underlying());
	}
};