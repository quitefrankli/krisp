#include <config.hpp>
#include <utility.hpp>
#include <gtest/gtest.h>


int main(int argc, char **argv) {
    Config::init("krisp_tests", RuntimePaths{
        .app_resources = std::filesystem::path(TEST_SOURCE_DIR) / "resources/default",
        .app_config = std::filesystem::path(TEST_SOURCE_DIR) / "configs",
        .engine_runtime = std::filesystem::path(TEST_BUILD_DIR) / "runtime",
        .writable_data = std::filesystem::path(TEST_BUILD_DIR) / "test-data/krisp_tests",
    });
    Utility::set_test_mode(std::filesystem::path(TEST_SOURCE_DIR) / "test/data");
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
