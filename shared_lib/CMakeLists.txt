cmake_minimum_required(VERSION 3.10)

# set the project name
project(shared_lib)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup()

find_package(glfw3 REQUIRED)
find_package(glm REQUIRED)

set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS 1)

set(CMAKE_CXX_STANDARD 17)
set(CMAkE_CXX_STANDARD_REQUIRED True)

file(GLOB_RECURSE SHARED_LIB_SRC 
	"src/*.cpp" "src/*.hpp"
	"../src/objects/*" "../src/vertex.cpp" "../src/shapes/*" "../src/maths.cpp")

add_library(shared_lib SHARED ${SHARED_LIB_SRC})

target_include_directories(shared_lib PRIVATE "../src/" "third_party/")