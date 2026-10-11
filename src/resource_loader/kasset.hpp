#pragma once

#include <filesystem>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

// Read-only access to a self-contained .kasset archive.
class KassetContainer
{
public:
	static std::shared_ptr<KassetContainer> open(const std::filesystem::path& path);

	const nlohmann::json& index() const noexcept { return document; }
	std::vector<unsigned char> read(std::string_view member) const;
	std::vector<unsigned char> read_prefix(std::string_view member, std::size_t size) const;

	// Normalize a package member path, rejecting paths which are not canonical
	// relative paths within the package.
	static std::string canonical_member_path(std::string_view path);

	// Resolve a URI found in `document_member` to a package member. Percent
	// escapes are decoded before containment is checked.
	static std::string resolve(std::string_view document_member, std::string_view uri);

private:
	struct FileEntry
	{
		std::uint64_t offset = 0;
		std::uint64_t size = 0;
	};

	KassetContainer(std::filesystem::path path, nlohmann::json document,
	                std::unordered_map<std::string, FileEntry> files);

	std::filesystem::path archive_path;
	nlohmann::json document;
	std::unordered_map<std::string, FileEntry> files;
};
