cmake_minimum_required(VERSION 3.20)

# set the project name
project(tetris)

file(GLOB_RECURSE TETRIS_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
add_executable(tetris ${TETRIS_SOURCES})
target_include_directories(tetris PRIVATE ${VulkanIncludes})
target_link_libraries(tetris VulkanLibs ${CONAN_LIBS})
