cmake_minimum_required(VERSION 3.20)

# set the project name
project(shared_lib)


file(GLOB_RECURSE SHARED_LIB_SRC 
	"src/*.cpp"
	"../src/objects/*" "../src/vertex.cpp" "../src/shapes/*" "../src/maths.cpp")

add_library(shared_lib SHARED ${SHARED_LIB_SRC})

# doesn't seem to work :(
# set_target_properties(shared_lib PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin")

target_include_directories(shared_lib PRIVATE "../src/" "third_party/")