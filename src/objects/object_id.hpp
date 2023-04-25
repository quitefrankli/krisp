#pragma once

#include <cstdint>
#include <cstdlib>
#include <functional>


struct ObjectID
{
public:
	explicit ObjectID(uint64_t id) : id(id) {}

	bool operator==(const ObjectID& other) const { return id == other.id; }
	bool operator!=(const ObjectID& other) const { return id != other.id; }
	bool operator<(const ObjectID& other) const { return id < other.id; }
	bool operator>(const ObjectID& other) const { return id > other.id; }

	ObjectID operator++() { return ObjectID(++id); }
	ObjectID operator++(int) { return ObjectID(id++); }

	uint64_t get_underlying() const { return id; }

private:
	uint64_t id;
};

template<>
struct std::hash<ObjectID>
{
	std::size_t operator()(const ObjectID& id) const
	{
		return std::hash<uint64_t>()(id.get_underlying());
	}
};