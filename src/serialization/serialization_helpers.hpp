#pragma once

#include "serialization/serializer.hpp"
#include "maths.hpp"

#include <glm/gtc/quaternion.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

#include <string_view>
#include <string>

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

inline void write_vec4(Serializer& out, const std::string_view key, const glm::vec4& value)
{
	auto map = out.map(key);
	map.write("x", value.x);
	map.write("y", value.y);
	map.write("z", value.z);
	map.write("w", value.w);
}

inline void write_mat4(Serializer& out, const std::string_view key, const glm::mat4& value)
{
	auto map = out.map(key);
	for (std::size_t column = 0; column < 4; ++column)
		write_vec4(map, "column_" + std::to_string(column), value[column]);
}

inline void write_transform(Serializer& out, const std::string_view key, const Maths::Transform& value)
{
	write_mat4(out, key, value.get_mat4());
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

inline glm::vec4 read_vec4(const Deserializer& in, const std::string_view key)
{
	const auto map = in.child(key);
	return { map.read<float>("x"), map.read<float>("y"), map.read<float>("z"), map.read<float>("w") };
}

inline glm::mat4 read_mat4(const Deserializer& in, const std::string_view key)
{
	const auto map = in.child(key);
	glm::mat4 value;
	for (std::size_t column = 0; column < 4; ++column)
		value[column] = read_vec4(map, "column_" + std::to_string(column));
	return value;
}

inline Maths::Transform read_transform(const Deserializer& in, const std::string_view key)
{
	return Maths::Transform(read_mat4(in, key));
}

inline glm::quat read_quat(const Deserializer& in, const std::string_view key)
{
	const auto map = in.child(key);
	return glm::quat(
		map.read<float>("w"), map.read<float>("x"), map.read<float>("y"), map.read<float>("z"));
}
}
