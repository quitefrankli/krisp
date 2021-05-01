cmake_minimum_required(VERSION 3.10)

# set the project name
project(Vulkan)

find_package(sdl2 CONFIG REQUIRED)

set(CMAKE_CXX_STANDARD 17)
set(CMAkE_CXX_STANDARD_REQUIRED True)

file(GLOB SOURCES "src/*.cpp" "src/*.hpp")

# add the executable
# add_executable(Display ${SOURCES})

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/ PROPERTY VS_STARTUP_PROJECT Vulkan)

add_executable(Vulkan ${SOURCES})
message(${CMAKE_CURRENT_SOURCE_DIR})

