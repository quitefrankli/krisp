cmake_minimum_required(VERSION 3.10)

# set the project name
project(Vulkan)

# # Dependency setup via conan.
# # Download conan executer in case it does not exists.
# if(NOT EXISTS "${CMAKE_CURRENT_BINARY_DIR}/conan.cmake")
#    message(STATUS "Downloading conan.cmake from https://github.com/conan-io/cmake-conan")
#    file(DOWNLOAD "https://raw.githubusercontent.com/conan-io/cmake-conan/v0.14/conan.cmake"
#                  "${CMAKE_CURRENT_BINARY_DIR}/conan.cmake")
# endif()
# # Execute conan build instructions.
# include(${CMAKE_CURRENT_BINARY_DIR}/conan.cmake)

# conan_cmake_run(CONANFILE conanfile.py
#                 BASIC_SETUP
#                 BUILD outdated
#                 PROFILE default
#                 PROFILE_AUTO build_type
#                 KEEP_RPATHS
# )

# if (NOT EXISTS "${CMAKE_BINARY_DIR}/conan.cmake")
#    message(STATUS "Downloading conan.cmake from https://github.com/conan-io/cmake-conan")
#    file(DOWNLOAD "https://raw.githubusercontent.com/conan-io/cmake-conan/v0.14/conan.cmake"
#                  "${CMAKE_BINARY_DIR}/conan.cmake")
# endif()

# include(${CMAKE_BINARY_DIR}/conan.cmake)
include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup()

# conan_cmake_run(CONANFILE conanfile.txt
#                 BASIC_SETUP CMAKE_TARGETS
# BUILD missing)

message("start")
message("${CMAKE_MODULE_PATH}")
message("${CMAKE_BINARY_DIR}")
message("${CONAN_INCLUDE_DIRS_GLFW}")
message("end")
# set(CMAKE_MODULE_PATH /home/user/Libs/cmake)
find_package(glfw3 REQUIRED)
find_package(glm REQUIRED)

set(CMAKE_CXX_STANDARD 17)
set(CMAkE_CXX_STANDARD_REQUIRED True)

file(GLOB SOURCES "src/*.cpp" "src/*.hpp")

# add the executable
# add_executable(Display ${SOURCES})

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/ PROPERTY VS_STARTUP_PROJECT Vulkan)

message(${CMAKE_CURRENT_SOURCE_DIR})
add_executable(Vulkan ${SOURCES})

# target_include_directories(Vulkan PUBLIC ${CONAN_INCLUDE_DIRS_GLFW})
target_link_libraries(Vulkan ${CONAN_LIBS})