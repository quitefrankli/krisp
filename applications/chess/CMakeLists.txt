cmake_minimum_required(VERSION 3.20)

# set the project name
project(chess)

file(GLOB_RECURSE CHESS_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
add_executable(chess ${CHESS_SOURCES})
target_include_directories(chess PRIVATE ${VulkanIncludes})
target_link_libraries(chess VulkanLibs ${CONAN_LIBS})
