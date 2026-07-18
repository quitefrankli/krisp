#pragma once

#include "serialization/serializer.hpp"

#include <glm/gtc/quaternion.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <string_view>

namespace EcsSerialization
{
inline void write_vec2(Serializer& out, const std::string_view key, const glm::vec2& value)
{
	auto map = out.map(key);
	map.write("x", value.x);
	map.write("y", value.y);
}

inline void write_vec3(Serializer& out, const std::string_view key, const glm::vec3& value)
{
	auto map = out.map(key);
	map.write("x", value.x);
	map.write("y", value.y);
	map.write("z", value.z);
}

inline void write_quat(Serializer& out, const std::string_view key, const glm::quat& value)
{
	auto map = out.map(key);
	map.write("x", value.x);
	map.write("y", value.y);
	map.write("z", value.z);
	map.write("w", value.w);
}

inline glm::vec2 read_vec2(const Deserializer& in, const std::string_view key)
{
	const auto map = in.child(key);
	return { map.read<float>("x"), map.read<float>("y") };
}

inline glm::vec3 read_vec3(const Deserializer& in, const std::string_view key)
{
	const auto map = in.child(key);
	return { map.read<float>("x"), map.read<float>("y"), map.read<float>("z") };
}

inline glm::quat read_quat(const Deserializer& in, const std::string_view key)
{
	const auto map = in.child(key);
	return glm::quat(
		map.read<float>("w"), map.read<float>("x"), map.read<float>("y"), map.read<float>("z"));
}
}
