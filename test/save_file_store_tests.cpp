#include "save_file_store.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>

namespace
{
class SaveFileStoreTests : public testing::Test
{
protected:
	void SetUp() override
	{
		root = std::filesystem::temp_directory_path()
			/ ("krisp_save_store_" + std::to_string(
				std::chrono::steady_clock::now().time_since_epoch().count()));
		std::filesystem::create_directories(root);
	}

	void TearDown() override
	{
		std::filesystem::remove_all(root);
	}

	void write(const std::string& name)
	{
		std::filesystem::create_directory(root / name);
		std::ofstream(root / name / "scene.yaml") << "scene: {}\n";
	}

	std::filesystem::path root;
};
}

TEST_F(SaveFileStoreTests, validates_names_and_resolves_save_paths)
{
	SaveFileStore store(root);
	EXPECT_EQ(store.path_for_overwrite(" My Save "), root / "My Save");

	for (const auto* invalid : { "", "   ", ".", "..", "slot.yaml", "a/b", "a\\b" })
		EXPECT_THROW(store.path_for_overwrite(invalid), std::invalid_argument);

	write("existing");
	EXPECT_EQ(store.path_for_overwrite("existing"), root / "existing");
}

TEST_F(SaveFileStoreTests, lists_only_yaml_files_newest_first)
{
	write("older");
	write("newer");
	std::ofstream(root / "ignored.yaml") << "scene: {}\n";
	std::filesystem::create_directory(root / "incomplete");
	const auto now = std::filesystem::file_time_type::clock::now();
	std::filesystem::last_write_time(root / "older" / "scene.yaml", now - std::chrono::hours(1));
	std::filesystem::last_write_time(root / "newer" / "scene.yaml", now);

	const auto entries = SaveFileStore(root).list();
	ASSERT_EQ(entries.size(), 2u);
	EXPECT_EQ(entries[0].name, "newer");
	EXPECT_EQ(entries[1].name, "older");
	EXPECT_FALSE(entries[0].modified_label.empty());
}

TEST_F(SaveFileStoreTests, discovers_and_deletes_quicksave_without_escaping_root)
{
	write("quicksave");
	std::ofstream(root / "quicksave" / "mesh_1.dat") << "payload";
	SaveFileStore store(root);
	ASSERT_EQ(store.list().front().name, "quicksave");
	EXPECT_TRUE(store.remove("quicksave"));
	EXPECT_FALSE(std::filesystem::exists(root / "quicksave"));
	EXPECT_THROW(store.remove("../outside"), std::invalid_argument);
}
