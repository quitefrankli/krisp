#include "save_file_store.hpp"

#include <algorithm>
#include <cctype>
#include <ctime>
#include <iomanip>
#include <ranges>
#include <sstream>
#include <stdexcept>

std::string SaveFileStore::validate_name(const std::string_view input)
{
	const auto first = std::ranges::find_if_not(input, [](const unsigned char c) { return std::isspace(c); });
	const auto last = std::ranges::find_if_not(input | std::views::reverse,
		[](const unsigned char c) { return std::isspace(c); }).base();
	if (first >= last)
		throw std::invalid_argument("Save name cannot be empty");

	const std::string name(first, last);
	if (name == "." || name == ".." || std::filesystem::path(name).has_extension())
		throw std::invalid_argument("Save name must not contain an extension");
	if (std::ranges::any_of(name, [](const unsigned char c) {
		return c < 32 || c == '/' || c == '\\';
	}))
		throw std::invalid_argument("Save name contains invalid characters");
	return name;
}

std::filesystem::path SaveFileStore::path_for(const std::string_view name) const
{
	return root / (validate_name(name) + ".yaml");
}

std::filesystem::path SaveFileStore::path_for_overwrite(const std::string_view name) const
{
	return path_for(name);
}

bool SaveFileStore::remove(const std::string_view name) const
{
	return std::filesystem::remove(path_for(name));
}

std::string SaveFileStore::format_modified(const std::filesystem::file_time_type modified)
{
	const auto system_time = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
		modified - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
	const std::time_t timestamp = std::chrono::system_clock::to_time_t(system_time);
	std::tm local{};
	localtime_r(&timestamp, &local);
	std::ostringstream label;
	label << std::put_time(&local, "%Y-%m-%d %H:%M");
	return label.str();
}

std::vector<SaveFileEntry> SaveFileStore::list() const
{
	std::vector<SaveFileEntry> entries;
	if (!std::filesystem::exists(root))
		return entries;

	for (const auto& item : std::filesystem::directory_iterator(root))
	{
		if (!item.is_regular_file() || item.path().extension() != ".yaml")
			continue;
		const auto modified = item.last_write_time();
		entries.push_back({ item.path().stem().string(), item.path(), modified, format_modified(modified) });
	}
	std::ranges::sort(entries, [](const auto& lhs, const auto& rhs) {
		return lhs.modified != rhs.modified ? lhs.modified > rhs.modified : lhs.name < rhs.name;
	});
	return entries;
}
