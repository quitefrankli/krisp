#pragma once

#include <yaml-cpp/yaml.h>

#include <concepts>
#include <cstdint>
#include <memory>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

class SerializationError : public std::runtime_error
{
public:
	using std::runtime_error::runtime_error;
};

enum class SerializationKind
{
	Null,
	Scalar,
	Sequence,
	Mapping,
};

namespace serialization_detail
{
template<typename T>
using Bare = std::remove_cvref_t<T>;

template<typename T>
concept Scalar = std::same_as<Bare<T>, bool>
	|| (std::integral<Bare<T>> && !std::same_as<Bare<T>, char>)
	|| std::floating_point<Bare<T>> || std::convertible_to<T, std::string_view>;

template<typename T>
concept ReadableScalar = std::same_as<Bare<T>, bool>
	|| (std::integral<Bare<T>> && !std::same_as<Bare<T>, char>)
	|| std::floating_point<Bare<T>> || std::same_as<Bare<T>, std::string>;

template<Scalar T>
YAML::Node make_node(const T& value)
{
	YAML::Node node;
	if constexpr (std::same_as<Bare<T>, bool>)
		node = value;
	else if constexpr (std::integral<Bare<T>> && std::is_signed_v<Bare<T>>)
		node = static_cast<std::int64_t>(value);
	else if constexpr (std::integral<Bare<T>>)
		node = static_cast<std::uint64_t>(value);
	else if constexpr (std::floating_point<Bare<T>>)
		node = static_cast<double>(value);
	else
		node = std::string(std::string_view(value));
	return node;
}
}

class Serializer
{
public:
	Serializer();

	template<serialization_detail::Scalar T>
	void write(const std::string_view key, const T& value)
	{
		write_node(key, serialization_detail::make_node(value));
	}

	void write_null(std::string_view key);
	Serializer map(std::string_view key);
	Serializer sequence(std::string_view key);

	template<serialization_detail::Scalar T>
	void append(const T& value)
	{
		append_node(serialization_detail::make_node(value));
	}

	void append_null();
	Serializer append_map();
	Serializer append_sequence();

	[[nodiscard]] std::string emit() const;

private:
	Serializer(std::shared_ptr<YAML::Node> document, YAML::Node node, std::string path);
	void write_node(std::string_view key, const YAML::Node& value);
	void append_node(const YAML::Node& value);
	[[nodiscard]] std::string child_path(std::string_view key) const;
	void require_mapping() const;
	void require_sequence() const;

	std::shared_ptr<YAML::Node> document_;
	YAML::Node node_;
	std::string path_;
};

class Deserializer
{
public:
	using Kind = SerializationKind;

	[[nodiscard]] static Deserializer parse(std::string_view yaml);
	[[nodiscard]] Deserializer child(std::string_view key) const;

	template<serialization_detail::ReadableScalar T>
	[[nodiscard]] serialization_detail::Bare<T> read(const std::string_view key) const
	{
		return child(key).template as<serialization_detail::Bare<T>>();
	}

	template<serialization_detail::ReadableScalar T>
	[[nodiscard]] serialization_detail::Bare<T> as() const
	{
		if (kind() != SerializationKind::Scalar)
			throw SerializationError("Expected scalar at " + path_);
		try {
			using Value = serialization_detail::Bare<T>;
			if constexpr (std::same_as<Value, std::string> || std::same_as<Value, bool>) {
				return node_.as<Value>();
			} else if constexpr (std::integral<Value> && std::is_signed_v<Value>) {
				const auto value = node_.as<std::int64_t>();
				if (value < std::numeric_limits<Value>::min() || value > std::numeric_limits<Value>::max())
					throw SerializationError("Integer out of range at " + path_);
				return static_cast<Value>(value);
			} else if constexpr (std::integral<Value>) {
				const auto value = node_.as<std::uint64_t>();
				if (value > std::numeric_limits<Value>::max())
					throw SerializationError("Integer out of range at " + path_);
				return static_cast<Value>(value);
			} else {
				return node_.as<Value>();
			}
		} catch (const YAML::Exception& error) {
			throw SerializationError("Invalid scalar at " + path_ + ": " + error.what());
		}
	}

	[[nodiscard]] std::vector<Deserializer> elements() const;
	[[nodiscard]] std::vector<std::string> keys() const;
	[[nodiscard]] SerializationKind kind() const;

private:
	Deserializer(std::shared_ptr<YAML::Node> document, YAML::Node node, std::string path);
	[[nodiscard]] std::string child_path(std::string_view key) const;

	std::shared_ptr<YAML::Node> document_;
	YAML::Node node_;
	std::string path_;
};
