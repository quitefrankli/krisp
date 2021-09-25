cmake_minimum_required(VERSION 3.10)

# set the project name
project(Vulkan)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup()

message("start")
message("${CMAKE_MODULE_PATH}")
message("${CMAKE_BINARY_DIR}")
message("${CONAN_INCLUDE_DIRS_GLFW}")
message("end")
find_package(glfw3 REQUIRED)
find_package(glm REQUIRED)

set(CMAKE_CXX_STANDARD 17)
set(CMAkE_CXX_STANDARD_REQUIRED True)
# set(CMAKE_CXX_FLAGS_DEBUG  "-g")

file(GLOB_RECURSE SOURCES "src/*.cpp" "src/*.hpp")

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/ PROPERTY VS_STARTUP_PROJECT Vulkan)

message(${CMAKE_CURRENT_SOURCE_DIR})
add_executable(Vulkan ${SOURCES})

target_include_directories(Vulkan PRIVATE "src/")
target_link_libraries(Vulkan ${CONAN_LIBS})