#include "test_helper.hpp"

#include <resource_loader/kasset.hpp>

#include <gtest/gtest.h>

#include <array>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace
{
void append_u32(std::vector<unsigned char>& bytes, const std::uint32_t value)
{
	for (unsigned shift = 0; shift < 32; shift += 8)
		bytes.push_back(static_cast<unsigned char>((value >> shift) & 0xff));
}

void append_u64(std::vector<unsigned char>& bytes, const std::uint64_t value)
{
	for (unsigned shift = 0; shift < 64; shift += 8)
		bytes.push_back(static_cast<unsigned char>((value >> shift) & 0xff));
}

std::filesystem::path write_archive(const nlohmann::json& index, const std::string_view payload)
{
	static std::uint64_t sequence = 0;
	const auto path = std::filesystem::temp_directory_path() /
		("krisp_kasset_test_" + std::to_string(sequence++) + ".kasset");
	std::vector<unsigned char> bytes{'K', 'A', 'S', 'S', 'E', 'T', 0, 0};
	append_u32(bytes, 1);
	append_u32(bytes, 0);
	auto absolute_offsets = index;
	std::vector<std::uint64_t> relative_offsets;
	for (auto& entry : absolute_offsets["files"])
		relative_offsets.push_back(entry.at("offset").get<std::uint64_t>());
	std::uint64_t base_offset = 24;
	std::string serialized;
	while (true)
	{
		for (std::size_t item = 0; item < relative_offsets.size(); ++item)
			absolute_offsets["files"][item]["offset"] = base_offset + relative_offsets[item];
		serialized = absolute_offsets.dump();
		const auto updated_base = 24 + serialized.size();
		if (updated_base == base_offset)
			break;
		base_offset = updated_base;
	}
	append_u64(bytes, serialized.size());
	bytes.insert(bytes.end(), serialized.begin(), serialized.end());
	bytes.insert(bytes.end(), payload.begin(), payload.end());
	std::ofstream stream(path, std::ios::binary | std::ios::trunc);
	stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
	return path;
}

nlohmann::json simple_index()
{
	return {
		{"files", {{{"path", "models/a.bin"}, {"offset", 0}, {"size", 3}}}},
		{"exports", {{{"name", "mesh/main"}, {"kind", "mesh"}, {"file", "models/a.bin"},
		              {"selector", {{"scene", 0}, {"node", 2}, {"primitive", 1}}}}}},
	};
}
}

TEST(KassetContainer, reads_indexed_members_and_exports)
{
	const auto path = write_archive(simple_index(), "abc");
	const auto archive = KassetContainer::open(path);
	EXPECT_EQ(archive->index().at("exports").at(0).at("name"), "mesh/main");
	EXPECT_EQ(archive->read("models/a.bin"), (std::vector<unsigned char>{'a', 'b', 'c'}));
	EXPECT_EQ(archive->read_prefix("models/a.bin", 2), (std::vector<unsigned char>{'a', 'b'}));
	EXPECT_EQ(archive->read_prefix("models/a.bin", 20), (std::vector<unsigned char>{'a', 'b', 'c'}));
	std::filesystem::remove(path);
}

TEST(KassetContainer, rejects_out_of_range_and_overlapping_members)
{
	auto index = simple_index();
	index["files"][0]["size"] = 4;
	const auto out_of_range = write_archive(index, "abc");
	EXPECT_THROW(KassetContainer::open(out_of_range), std::runtime_error);
	std::filesystem::remove(out_of_range);

	index = simple_index();
	index["files"].push_back({{"path", "models/b.bin"}, {"offset", 2}, {"size", 1}});
	const auto overlap = write_archive(index, "abcd");
	EXPECT_THROW(KassetContainer::open(overlap), std::runtime_error);
	std::filesystem::remove(overlap);
}

TEST(KassetContainer, rejects_exports_with_missing_files_or_duplicate_names)
{
	auto index = simple_index();
	index["exports"][0]["file"] = "missing.bin";
	const auto missing = write_archive(index, "abc");
	EXPECT_THROW(KassetContainer::open(missing), std::runtime_error);
	std::filesystem::remove(missing);

	index = simple_index();
	index["exports"].push_back(index["exports"][0]);
	const auto duplicate = write_archive(index, "abc");
	EXPECT_THROW(KassetContainer::open(duplicate), std::runtime_error);
	std::filesystem::remove(duplicate);
}

TEST(KassetContainer, resolves_relative_uris_after_decoding_and_enforces_containment)
{
	EXPECT_EQ(KassetContainer::resolve("models/scene.gltf", "../textures/albedo%20map.png"),
	          "textures/albedo map.png");
	EXPECT_THROW(KassetContainer::resolve("scene.gltf", "../../outside.bin"), std::runtime_error);
	EXPECT_THROW(KassetContainer::resolve("scene.gltf", "https://example.invalid/data.bin"), std::runtime_error);
	EXPECT_THROW(KassetContainer::canonical_member_path("../outside.bin"), std::runtime_error);
}
