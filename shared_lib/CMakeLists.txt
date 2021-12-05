cmake_minimum_required(VERSION 3.10)

# set the project name
project(shared_lib)

# include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
# conan_basic_setup()

# find_package(glfw3 REQUIRED)
# find_package(glm REQUIRED)

# set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS 1)

# set(CMAKE_CXX_STANDARD 17)
# set(CMAkE_CXX_STANDARD_REQUIRED True)
# set(CMAKE_CXX_FLAGS_DEBUG  "-g")
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS 1)

# file(GLOB_RECURSE SOURCES "src/*.cpp" "src/*.hpp")

# set_property(GLOBAL PROPERTY USE_FOLDERS ON)
# set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/ PROPERTY VS_STARTUP_PROJECT Vulkan)

# add_executable(Vulkan ${SOURCES})

# target_include_directories(Vulkan PRIVATE "src/")
# target_link_libraries(Vulkan ${CONAN_LIBS})

# add_subdirectory(shared_lib)

# set(SHARED_LIB_SRC ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
set(CMAKE_BUILD_TYPE Release)

file(GLOB_RECURSE SHARED_LIB_SRC "src/*.cpp" "src/*.hpp")

add_library(shared_lib SHARED ${SHARED_LIB_SRC})