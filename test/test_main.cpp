#include <config.hpp>
#include <resource_paths.hpp>
#include <utility.hpp>
#include <gtest/gtest.h>

#include <filesystem>

int main(int argc, char **argv) {
    auto paths = ResourcePaths::paths_for_executable("krisp_tests");
    paths.app_resources = std::filesystem::path(TEST_SOURCE_DIR) / "resources/default";
    paths.app_config = std::filesystem::path(TEST_SOURCE_DIR) / "configs";
    paths.engine.resources = std::filesystem::path(TEST_BUILD_DIR) / "test-data/engine-resources";
    paths.writable_data = std::filesystem::path(TEST_BUILD_DIR) / "test-data/krisp_tests";
    std::filesystem::create_directories(paths.engine.resources);
    if (!std::filesystem::exists(paths.engine.resources / "textures"))
        std::filesystem::create_directory_symlink(
            std::filesystem::path(TEST_SOURCE_DIR) / "resources/default/textures",
            paths.engine.resources / "textures");
    Config::init("krisp_tests", std::move(paths));
    ResourcePaths::set_test_mode(std::filesystem::path(TEST_SOURCE_DIR) / "test/data");
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
