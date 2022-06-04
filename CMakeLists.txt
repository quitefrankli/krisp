cmake_minimum_required(VERSION 3.20)

# set the project name
project(Vulkan)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup()

set(CMAKE_CXX_STANDARD 17)
set(CMAkE_CXX_STANDARD_REQUIRED True)

if(WIN32)
	# for MSVC this will set the limit of max simultaneous TU compiles
	set(CMAKE_CXX_FLAGS${build_type} "${CMAKE_CXX_FLAGS${build_type}} /MP12")
else()
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g")
endif()

# set(CMAKE_CXX_FLAGS_DEBUG  "-g")

add_definitions(-DGLM_FORCE_DEPTH_ZERO_TO_ONE) # vulkan uses the z range -> [0, 1] as opposed to [-1, 1]
add_definitions(-DPROJECT_TOP_LEVEL_SRC_DIR="${CMAKE_SOURCE_DIR}")
add_definitions(-DPROJECT_BUILD_DIR="${CMAKE_BINARY_DIR}")
add_definitions(-DPROJECT_BIN_DIR="${CMAKE_BINARY_DIR}/bin")

if(DISABLE_SLEEP)
	add_definitions(-DDISABLE_SLEEP)
endif()

file(GLOB_RECURSE 
	 VulkanSources 
	 ${CMAKE_SOURCE_DIR}/src/*.cpp
	 ${CMAKE_SOURCE_DIR}/third_party/*.cpp)
# remove main.cpp from VulkanSources
list(FILTER VulkanSources EXCLUDE REGEX "main.cpp")

set(VulkanIncludes 
	${CMAKE_SOURCE_DIR}/src/ 
	${CMAKE_SOURCE_DIR}/third_party/ 
	${CMAKE_SOURCE_DIR}/shared_lib/src/)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/ PROPERTY VS_STARTUP_PROJECT Vulkan)

add_library(VulkanLibs ${VulkanSources})
# public keyword means that stuff that depends on VulkanLibs will also inherit this include
target_include_directories(VulkanLibs PUBLIC ${VulkanIncludes})

add_executable(Vulkan ${CMAKE_SOURCE_DIR}/src/main.cpp) # bit annoying but seems we need to include at least 1 source file
target_link_libraries(Vulkan VulkanLibs ${CONAN_LIBS})

add_subdirectory(applications)
add_subdirectory(shared_lib)
enable_testing()

# could be useful for recompiling shaders or dynamic library
# add_custom_command(
# 	OUTPUT None
# 	COMMAND cat testfile.txt
# 	DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/README.md
# 	VERBATIM
# )

# target_link_libraries(Vulkan ${CONAN_LIBS} shared_lib) # this links a start-time dynamic lib when we actually want a run-time dynamic lib
# target_include_directories(Vulkan PRIVATE "src/" "shared_lib/src/")