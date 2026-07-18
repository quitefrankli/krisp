#pragma once

#include "objects/object.hpp"

#include <memory>
#include <string_view>

namespace TypeRegistry
{
using Factory = std::shared_ptr<Object> (*)();

template<typename ObjectT>
struct DefaultFactory
{
	static std::shared_ptr<Object> create()
	{
		return std::make_shared<ObjectT>();
	}
};

bool contains(std::string_view name);
std::shared_ptr<Object> create(std::string_view name);
}
