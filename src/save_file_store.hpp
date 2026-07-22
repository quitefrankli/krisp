#pragma once

#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

struct SaveFileEntry
{
	std::string name;
	std::filesystem::path path;
	std::filesystem::file_time_type modified;
	std::string modified_label;
};

class SaveFileStore
{
public:
	explicit SaveFileStore(std::filesystem::path root) : root(std::move(root)) {}

	[[nodiscard]] std::vector<SaveFileEntry> list() const;
	[[nodiscard]] std::filesystem::path path_for_overwrite(std::string_view name) const;
	bool remove(std::string_view name) const;

private:
	[[nodiscard]] std::filesystem::path path_for(std::string_view name) const;
	static std::string validate_name(std::string_view name);
	static std::string format_modified(std::filesystem::file_time_type modified);

	std::filesystem::path root;
};
