#include "serializer.hpp"

#include <cctype>
#include <utility>

namespace
{
std::string mapping_path(const std::string& parent, const std::string_view key)
{
	const bool identifier = !key.empty()
		&& (std::isalpha(static_cast<unsigned char>(key.front())) || key.front() == '_')
		&& [&] {
			for (const char character : key) {
				if (!std::isalnum(static_cast<unsigned char>(character)) && character != '_')
					return false;
			}
			return true;
		}();
	if (identifier)
		return parent + "." + std::string(key);
	return parent + "[\"" + std::string(key) + "\"]";
}
}

Serializer::Serializer()
	: document_(std::make_shared<YAML::Node>(YAML::NodeType::Map)), node_(*document_), path_("$")
{
}

Serializer::Serializer(std::shared_ptr<YAML::Node> document, YAML::Node node, std::string path)
	: document_(std::move(document)), node_(std::move(node)), path_(std::move(path))
{
}

void Serializer::write_null(const std::string_view key)
{
	write_node(key, YAML::Node(YAML::NodeType::Null));
}

Serializer Serializer::map(const std::string_view key)
{
	YAML::Node value(YAML::NodeType::Map);
	write_node(key, value);
	return Serializer(document_, node_[std::string(key)], child_path(key));
}

Serializer Serializer::sequence(const std::string_view key)
{
	YAML::Node value(YAML::NodeType::Sequence);
	write_node(key, value);
	return Serializer(document_, node_[std::string(key)], child_path(key));
}

void Serializer::append_null()
{
	append_node(YAML::Node(YAML::NodeType::Null));
}

Serializer Serializer::append_map()
{
	require_sequence();
	const auto index = node_.size();
	node_.push_back(YAML::Node(YAML::NodeType::Map));
	return Serializer(document_, node_[index], path_ + "[" + std::to_string(index) + "]");
}

Serializer Serializer::append_sequence()
{
	require_sequence();
	const auto index = node_.size();
	node_.push_back(YAML::Node(YAML::NodeType::Sequence));
	return Serializer(document_, node_[index], path_ + "[" + std::to_string(index) + "]");
}

std::string Serializer::emit() const
{
	try {
		YAML::Emitter emitter;
		emitter << *document_;
		if (!emitter.good())
			throw SerializationError("Failed to emit YAML at $: " + emitter.GetLastError());
		return emitter.c_str();
	} catch (const YAML::Exception& error) {
		throw SerializationError("Failed to emit YAML at $: " + std::string(error.what()));
	}
}

void Serializer::write_node(const std::string_view key, const YAML::Node& value)
{
	require_mapping();
	for (const auto& entry : node_) {
		if (entry.first.as<std::string>() == key)
			throw SerializationError("Duplicate mapping key at " + child_path(key));
	}
	node_[std::string(key)] = value;
}

void Serializer::append_node(const YAML::Node& value)
{
	require_sequence();
	node_.push_back(value);
}

std::string Serializer::child_path(const std::string_view key) const
{
	return mapping_path(path_, key);
}

void Serializer::require_mapping() const
{
	if (!node_.IsMap())
		throw SerializationError("Expected mapping at " + path_);
}

void Serializer::require_sequence() const
{
	if (!node_.IsSequence())
		throw SerializationError("Expected sequence at " + path_);
}

Deserializer::Deserializer(std::shared_ptr<YAML::Node> document, YAML::Node node, std::string path)
	: document_(std::move(document)), node_(std::move(node)), path_(std::move(path))
{
}

Deserializer Deserializer::parse(const std::string_view yaml)
{
	try {
		auto document = std::make_shared<YAML::Node>(YAML::Load(std::string(yaml)));
		return Deserializer(document, *document, "$");
	} catch (const YAML::Exception& error) {
		throw SerializationError("Invalid YAML at $: " + std::string(error.what()));
	}
}

Deserializer Deserializer::child(const std::string_view key) const
{
	const auto path = child_path(key);
	if (!node_.IsMap())
		throw SerializationError("Expected mapping at " + path_ + " while reading " + path);
	const YAML::Node value = node_[std::string(key)];
	if (!value.IsDefined())
		throw SerializationError("Missing field at " + path);
	return Deserializer(document_, value, path);
}

std::vector<Deserializer> Deserializer::elements() const
{
	if (!node_.IsSequence())
		throw SerializationError("Expected sequence at " + path_);
	std::vector<Deserializer> result;
	result.reserve(node_.size());
	for (std::size_t index = 0; index < node_.size(); ++index)
		result.push_back(Deserializer(document_, node_[index], path_ + "[" + std::to_string(index) + "]"));
	return result;
}

std::vector<std::string> Deserializer::keys() const
{
	if (!node_.IsMap())
		throw SerializationError("Expected mapping at " + path_);
	std::vector<std::string> result;
	result.reserve(node_.size());
	for (const auto& entry : node_) {
		try {
			result.push_back(entry.first.as<std::string>());
		} catch (const YAML::Exception& error) {
			throw SerializationError("Invalid mapping key at " + path_ + ": " + error.what());
		}
	}
	return result;
}

SerializationKind Deserializer::kind() const
{
	if (node_.IsNull())
		return SerializationKind::Null;
	if (node_.IsScalar())
		return SerializationKind::Scalar;
	if (node_.IsSequence())
		return SerializationKind::Sequence;
	if (node_.IsMap())
		return SerializationKind::Mapping;
	throw SerializationError("Undefined YAML node at " + path_);
}

std::string Deserializer::child_path(const std::string_view key) const
{
	return mapping_path(path_, key);
}
