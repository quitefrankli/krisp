cmake_minimum_required(VERSION 3.20)

# set the project name
project(benchmark)

file(GLOB_RECURSE BENCHMARK_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
add_executable(benchmark ${BENCHMARK_SOURCES})
target_include_directories(benchmark PRIVATE ${VulkanIncludes})
target_link_libraries(benchmark VulkanLibs ${CONAN_LIBS})
