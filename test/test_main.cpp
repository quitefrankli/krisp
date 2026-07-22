#include <config.hpp>
#include <utility.hpp>
#include <gtest/gtest.h>


int main(int argc, char **argv) {
    Config::init("krisp_tests");
    Utility::set_test_mode();
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
