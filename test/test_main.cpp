#include <config.hpp>
#include <gtest/gtest.h>


int main(int argc, char **argv) {
    Config::init("krisp_tests");
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}