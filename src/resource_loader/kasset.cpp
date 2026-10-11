#include "kasset.hpp"

#include <algorithm>
#include <array>
#include <fstream>
#include <limits>
#include <ranges>
#include <set>
#include <stdexcept>
#include <utility>

namespace
{
constexpr std::array<unsigned char, 8> MAGIC{'K', 'A', 'S', 'S', 'E', 'T', 0, 0};
constexpr std::uint32_t VERSION = 1;
constexpr std::uint32_t FLAGS = 0;
constexpr std::uint64_t MAX_INDEX_SIZE = 16 * 1024 * 1024;

class BinaryInput
{
public:
	explicit BinaryInput(std::ifstream& stream) : stream(stream) {}

	std::uint32_t u32()
	{
		std::array<unsigned char, 4> bytes{};
		read(bytes.data(), bytes.size());
		std::uint32_t value = 0;
		for (unsigned index = 0; index < bytes.size(); ++index)
			value |= static_cast<std::uint32_t>(bytes[index]) << (index * 8);
		return value;
	}

	std::uint64_t u64()
	{
		std::array<unsigned char, 8> bytes{};
		read(bytes.data(), bytes.size());
		std::uint64_t value = 0;
		for (unsigned index = 0; index < bytes.size(); ++index)
			value |= static_cast<std::uint64_t>(bytes[index]) << (index * 8);
		return value;
	}

	void read(unsigned char* destination, const std::size_t size)
	{
		if (!stream.read(reinterpret_cast<char*>(destination), static_cast<std::streamsize>(size)))
			throw std::runtime_error("Truncated .kasset header or index");
	}

private:
	std::ifstream& stream;
};

std::string required_string(const nlohmann::json& value, const char* key)
{
	if (!value.contains(key) || !value.at(key).is_string())
		throw std::runtime_error(std::string("Invalid .kasset index: expected string ") + key);
	return value.at(key).get<std::string>();
}

std::uint64_t required_u64(const nlohmann::json& value, const char* key)
{
	if (!value.contains(key) || !value.at(key).is_number_unsigned())
		throw std::runtime_error(std::string("Invalid .kasset index: expected unsigned integer ") + key);
	return value.at(key).get<std::uint64_t>();
}

int hex_value(const char value)
{
	if (value >= '0' && value <= '9') return value - '0';
	if (value >= 'a' && value <= 'f') return value - 'a' + 10;
	if (value >= 'A' && value <= 'F') return value - 'A' + 10;
	return -1;
}

std::string decode_uri_path(const std::string_view value)
{
	std::string decoded;
	decoded.reserve(value.size());
	for (std::size_t index = 0; index < value.size(); ++index)
	{
		if (value[index] != '%')
		{
			decoded.push_back(value[index]);
			continue;
		}
		if (index + 2 >= value.size())
			throw std::runtime_error("Malformed percent escape in .kasset URI");
		const int high = hex_value(value[index + 1]);
		const int low = hex_value(value[index + 2]);
		if (high < 0 || low < 0)
			throw std::runtime_error("Malformed percent escape in .kasset URI");
		const char decoded_byte = static_cast<char>((high << 4) | low);
		if (decoded_byte == '\0')
			throw std::runtime_error("NUL byte in .kasset URI");
		decoded.push_back(decoded_byte);
		index += 2;
	}
	return decoded;
}

bool is_export_kind(const std::string& kind)
{
	static const std::set<std::string> kinds{
		"mesh", "material", "texture", "skeleton", "animation", "assembly"};
	return kinds.contains(kind);
}
}

KassetContainer::KassetContainer(
	std::filesystem::path path,
	nlohmann::json document,
	std::unordered_map<std::string, FileEntry> files) :
	archive_path(std::move(path)), document(std::move(document)),
	files(std::move(files))
{
}

std::shared_ptr<KassetContainer> KassetContainer::open(const std::filesystem::path& path)
{
	std::ifstream stream(path, std::ios::binary);
	if (!stream)
		throw std::runtime_error("Unable to open .kasset file: " + path.string());

	BinaryInput input(stream);
	std::array<unsigned char, MAGIC.size()> magic{};
	input.read(magic.data(), magic.size());
	if (magic != MAGIC)
		throw std::runtime_error("Invalid .kasset magic");
	if (input.u32() != VERSION)
		throw std::runtime_error("Unsupported .kasset version");
	if (input.u32() != FLAGS)
		throw std::runtime_error("Unsupported .kasset flags");
	const auto index_size = input.u64();
	if (index_size > MAX_INDEX_SIZE || index_size > std::numeric_limits<std::size_t>::max())
		throw std::runtime_error(".kasset index exceeds the size limit");
	const auto payload_offset = static_cast<std::uint64_t>(MAGIC.size() + 4 + 4 + 8) + index_size;
	std::string json_bytes(static_cast<std::size_t>(index_size), '\0');
	input.read(reinterpret_cast<unsigned char*>(json_bytes.data()), json_bytes.size());
	nlohmann::json document;
	try
	{
		document = nlohmann::json::parse(json_bytes);
	}
	catch (const nlohmann::json::exception& error)
	{
		throw std::runtime_error(std::string("Invalid .kasset JSON index: ") + error.what());
	}
	if (!document.is_object() || !document.contains("files") || !document.at("files").is_array() ||
	    !document.contains("exports") || !document.at("exports").is_array())
		throw std::runtime_error("Invalid .kasset index: expected files and exports arrays");

	stream.seekg(0, std::ios::end);
	const auto end_position = stream.tellg();
	if (end_position < 0 || static_cast<std::uint64_t>(end_position) < payload_offset)
		throw std::runtime_error("Truncated .kasset payload");
	const auto file_size = static_cast<std::uint64_t>(end_position);
	std::unordered_map<std::string, FileEntry> files;
	std::vector<std::pair<std::uint64_t, std::uint64_t>> ranges;
	for (const auto& entry : document.at("files"))
	{
		if (!entry.is_object())
			throw std::runtime_error("Invalid .kasset file entry");
		const auto path_value = required_string(entry, "path");
		const auto canonical = canonical_member_path(path_value);
		if (canonical != path_value)
			throw std::runtime_error("Non-canonical .kasset member path: " + path_value);
		const auto offset = required_u64(entry, "offset");
		const auto size = required_u64(entry, "size");
		if (offset < payload_offset || offset > file_size || size > file_size - offset)
			throw std::runtime_error(".kasset member range is outside the payload: " + path_value);
		if (!files.emplace(path_value, FileEntry{offset, size}).second)
			throw std::runtime_error("Duplicate .kasset member path: " + path_value);
		ranges.emplace_back(offset, offset + size);
	}
	std::ranges::sort(ranges);
	for (std::size_t index = 1; index < ranges.size(); ++index)
		if (ranges[index].first < ranges[index - 1].second)
			throw std::runtime_error("Overlapping .kasset member ranges");

	std::set<std::string> export_names;
	for (const auto& entry : document.at("exports"))
	{
		if (!entry.is_object())
			throw std::runtime_error("Invalid .kasset export entry");
		const auto name = required_string(entry, "name");
		const auto kind = required_string(entry, "kind");
		const auto file = required_string(entry, "file");
		if (name.empty() || !export_names.insert(name).second)
			throw std::runtime_error("Duplicate or empty .kasset export name: " + name);
		if (!is_export_kind(kind))
			throw std::runtime_error("Unsupported .kasset export kind: " + kind);
		const auto canonical = canonical_member_path(file);
		if (canonical != file || !files.contains(file))
			throw std::runtime_error(".kasset export references a missing file: " + file);
		if (!entry.contains("selector") || !entry.at("selector").is_object())
			throw std::runtime_error("Invalid .kasset export selector");
		const auto& selector = entry.at("selector");
		static const std::set<std::string> fields{
			"scene", "node", "primitive", "material", "image", "skin", "animation"};
		for (auto it = selector.begin(); it != selector.end(); ++it)
			if (!fields.contains(it.key()) || !it.value().is_number_integer())
				throw std::runtime_error("Invalid .kasset export selector field: " + it.key());
	}

	return std::shared_ptr<KassetContainer>(new KassetContainer(
		std::filesystem::absolute(path), std::move(document), std::move(files)));
}

std::vector<unsigned char> KassetContainer::read(const std::string_view member) const
{
	const auto canonical = canonical_member_path(member);
	if (canonical != member)
		throw std::runtime_error("Non-canonical .kasset member path: " + std::string(member));
	const auto found = files.find(canonical);
	if (found == files.end())
		throw std::runtime_error("Missing .kasset member: " + canonical);
	if (found->second.size > std::numeric_limits<std::size_t>::max())
		throw std::runtime_error(".kasset member is too large: " + canonical);
	return read_prefix(canonical, static_cast<std::size_t>(found->second.size));
}

std::vector<unsigned char> KassetContainer::read_prefix(
	const std::string_view member, const std::size_t requested_size) const
{
	const auto canonical = canonical_member_path(member);
	if (canonical != member)
		throw std::runtime_error("Non-canonical .kasset member path: " + std::string(member));
	const auto found = files.find(canonical);
	if (found == files.end())
		throw std::runtime_error("Missing .kasset member: " + canonical);
	const auto read_size = std::min<std::uint64_t>(requested_size, found->second.size);
	if (read_size > std::numeric_limits<std::size_t>::max() ||
	    read_size > static_cast<std::uint64_t>(std::numeric_limits<std::streamsize>::max()) ||
	    found->second.offset > static_cast<std::uint64_t>(std::numeric_limits<std::streamoff>::max()))
		throw std::runtime_error(".kasset member prefix is too large: " + canonical);
	std::ifstream stream(archive_path, std::ios::binary);
	if (!stream)
		throw std::runtime_error("Unable to open .kasset file: " + archive_path.string());
	stream.seekg(static_cast<std::streamoff>(found->second.offset));
	std::vector<unsigned char> bytes(static_cast<std::size_t>(read_size));
	if (!bytes.empty() && !stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size())))
		throw std::runtime_error("Unable to read .kasset member: " + canonical);
	return bytes;
}

std::string KassetContainer::canonical_member_path(const std::string_view path)
{
	if (path.empty() || path.front() == '/' || path.find('\\') != std::string_view::npos ||
	    path.find('\0') != std::string_view::npos || path.find(':') != std::string_view::npos)
		throw std::runtime_error("Invalid .kasset member path");
	std::size_t start = 0;
	while (start < path.size())
	{
		const auto end = path.find('/', start);
		const auto segment = path.substr(start, end == std::string_view::npos ? path.size() - start : end - start);
		if (segment.empty() || segment == "." || segment == "..")
			throw std::runtime_error("Invalid .kasset member path");
		start = end == std::string_view::npos ? path.size() : end + 1;
	}
	if (path.back() == '/')
		throw std::runtime_error("Invalid .kasset member path");
	return std::string(path);
}

std::string KassetContainer::resolve(const std::string_view document_member, const std::string_view uri)
{
	const auto document_path = canonical_member_path(document_member);
	if (uri.empty())
		throw std::runtime_error("Empty URI in .kasset document");
	auto decoded = decode_uri_path(uri);
	if (decoded.find('?') != std::string::npos || decoded.find('#') != std::string::npos)
		throw std::runtime_error("Query or fragment is not supported in .kasset URI");
	if (decoded.find(':') != std::string::npos || decoded.front() == '/' || decoded.starts_with("//"))
		throw std::runtime_error("External URI is not contained in .kasset");
	if (decoded.back() == '/')
		throw std::runtime_error("URI does not name a .kasset member");

	std::vector<std::string> parts;
	const auto directory_end = document_path.rfind('/');
	if (directory_end != std::string::npos)
	{
		std::size_t start = 0;
		while (start < directory_end)
		{
			const auto end = document_path.find('/', start);
			parts.emplace_back(document_path.substr(start, end == std::string::npos ? directory_end - start : end - start));
			start = end == std::string::npos ? directory_end : end + 1;
		}
	}
	std::size_t start = 0;
	while (start <= decoded.size())
	{
		const auto end = decoded.find('/', start);
		const auto segment = decoded.substr(start, end == std::string::npos ? decoded.size() - start : end - start);
		if (segment.empty() || segment == ".")
		{
			// Repeated separators and dot segments do not change the member path.
		}
		else if (segment == "..")
		{
			if (parts.empty())
				throw std::runtime_error("URI escapes .kasset package");
			parts.pop_back();
		}
		else
			parts.push_back(segment);
		if (end == std::string::npos)
			break;
		start = end + 1;
	}
	if (parts.empty())
		throw std::runtime_error("Invalid URI path in .kasset document");
	std::string result;
	for (const auto& part : parts)
	{
		if (!result.empty()) result.push_back('/');
		result += part;
	}
	return canonical_member_path(result);
}
