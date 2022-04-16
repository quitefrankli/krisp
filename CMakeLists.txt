cmake_minimum_required(VERSION 3.10)

# set the project name
project(Vulkan)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup()

set(CMAKE_CXX_STANDARD 17)
set(CMAkE_CXX_STANDARD_REQUIRED True)
# set(CMAKE_CXX_FLAGS_DEBUG  "-g")
set(CMAKE_GENERATOR_PLATFORM x64)

add_subdirectory(shared_lib)

include_directories(AFTER "shared_lib/src")
file(GLOB_RECURSE SOURCES "src/*.cpp" "src/*.hpp" "third_party/*.cpp" "third_party/*.h")

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/ PROPERTY VS_STARTUP_PROJECT Vulkan)
add_executable(Vulkan ${SOURCES})

target_include_directories(Vulkan PRIVATE "src/" "third_party/")
target_link_libraries(Vulkan ${CONAN_LIBS})

add_definitions(-DGLM_FORCE_DEPTH_ZERO_TO_ONE) # vulkan uses the z range -> [0, 1] as opposed to [-1, 1]

# could be useful for recompiling shaders or dynamic library
# add_custom_command(
# 	OUTPUT None
# 	COMMAND cat testfile.txt
# 	DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/README.md
# 	VERBATIM
# )

# target_link_libraries(Vulkan ${CONAN_LIBS} shared_lib) # this links a start-time dynamic lib when we actually want a run-time dynamic lib
# target_include_directories(Vulkan PRIVATE "src/" "shared_lib/src/")